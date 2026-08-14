# MCP guide: find `RaycastBoundDesc` / `RaycastBoundFn` after a Roblox update

Use this when an AI has **IDA Pro MCP** connected to a fresh `RobloxPlayerBeta.exe` IDB.
Goal: fill `Offsets::WorldRoot::RaycastBoundDesc` and `RaycastBoundFn` in `src/core/roblox/offsets/Offsets.h`.

Do **not** invent addresses. Every RVA must come from the open IDB (or a live process read).

---

## Hard facts (do not violate)

1. `BoundFuncDesc` for WorldRoot methods sits in **high `.data` / BSS**.
2. On disk / in a cold IDB the desc body is often **all `0x00` or `0xFF`**.
   - `*(desc + 8)` (name) and `*(desc + 0x80)` (fn) are filled **at runtime**.
3. Therefore these heuristics are **wrong** for this layout:
   - “xref to string `Raycast` → `place - 8` = BoundFuncDesc”
   - “scan for name pointer at `desc+8` in static file”
4. Correct static method:
   - WorldRoot **method-name table** (string pointers)
   - WorldRoot **BoundFuncDesc pointer table** (pointers into `.data`)
   - Same **order** and usually same **count** → index of `"Raycast"` selects the desc pointer.
5. `RaycastBoundFn` is almost always **`0x80`**. Confirm live if a debugger/process is attached; otherwise keep `0x80`.

Current expected shape (example only — values change every update):

```cpp
inline constexpr uintptr_t RaycastBoundDesc = 0x........; // RVA
inline constexpr uintptr_t RaycastBoundFn   = 0x80;
```

Cheat uses: `slot = module_base + RaycastBoundDesc + RaycastBoundFn`.

---

## Preferred path — run the IDA script

1. Open the matching IDB for the target Roblox build.
2. Run `tools/ida/raycast.py` — File → Script file… / MCP `py_exec_file`.
3. Copy printed lines into `Offsets.h`:
   - `WorldRoot::RaycastBoundDesc` / `RaycastBoundFn`
   - `FastClusterEntity::VTableRva` (engine chams)

If the script output looks sane (Raycast index maps to a high RVA, nearby methods listed, FastCluster slot0 in `.text`), **stop**. Done.

---

## Path A — find via IDA MCP tools (for an AI)

Server: `user-ida-pro-mcp`.

Always discover tool schemas with `GetMcpTools` before calling unfamiliar tools.
Keep outputs small. Prefer `py_eval` for multi-step scans; use `find` / `xrefs_to` only for confirmation.

### Step 0 — health + imagebase

```text
server_health
py_eval: print(hex(ida_nalt.get_imagebase()))
```

Note `imagebase` (often `0x140000000`). All RVAs = `VA - imagebase`.

### Step 1 — confirm exact strings exist

Search exact C-strings (not substrings):

- `WorldRoot`
- `Raycast`
- `FindPartOnRay`
- optionally `Blockcast`, `Spherecast`

MCP options:

- `find` / `find_regex` / `search_text` for the string
- or `py_eval` over `idautils.Strings()` with `str(s) == "Raycast"`

If `"Raycast"` or `"WorldRoot"` is missing, wrong binary / stripped IDB — stop.

### Step 2 — recover WorldRoot method-name table

Pattern in `.rdata` (or similar): repeating pairs

```text
qword: pointer to "WorldRoot"
qword: pointer to MethodName   // ArePartsTouchingOthers, Blockcast, ..., Raycast, ...
```

Algorithm via `py_eval`:

1. Find VA of exact string `"WorldRoot"`.
2. `bin_search` for that 8-byte little-endian pointer across non-code (or whole image).
   - IDA 9.x: `parse_binpat_str` returns `""` on success; check `len(pats)`.
   - `bin_search` may return `(ea, status)` — use `ea = r[0]`.
3. At each hit `H`, try walking:
   - mode `cm`: `cstr(*(H)) == "WorldRoot"` and `cstr(*(H+8))` is a method name; step `+0x10`
   - mode `mc`: swapped order
4. Read C-strings **byte-by-byte until `\\0`**. Do **not** use `get_strlit_contents` — IDA merges adjacent rdata names and breaks matching.
5. Keep the **longest** run that contains **all** of:
   - `Raycast`
   - `FindPartOnRay`
   - at least two of `Blockcast` / `Spherecast` / `Shapecast`
6. Record:
   - `methods[]` list
   - `idx = methods.index("Raycast")`

Sanity: typically ~25–40 methods. Index of `Raycast` is usually mid/late in the list.

### Step 3 — recover BoundFuncDesc pointer table

Near the **method name string cluster** (same area as `"FindPartOnRay"`, `"Raycast"`, `"Spherecast"`):

Just **before** those strings there is a table of consecutive qwords:

```text
qword: pointer into high .data  // BoundFuncDesc for methods[0]
qword: pointer into high .data  // methods[1]
...
```

Algorithm via `py_eval`:

1. Take VA of exact `"FindPartOnRay"` (fallback: `"Raycast"`).
2. Scan backward ~`0x400` bytes, 8-byte aligned.
3. Collect the longest run of qwords `V` where:
   - `V` is inside the image
   - target segment is **not executable**
   - RVA `(V - imagebase)` is **high** (this build: often `> 0x4000000`, commonly `0x8xxxxxx`)
4. Prefer a run whose **length == len(methods)**.
5. Then:

```text
desc_va  = table[idx]
desc_rva = desc_va - imagebase
```

### Step 4 — verify

Must hold:

- `methods[idx] == "Raycast"`
- `len(table) == len(methods)` (or explain a tiny mismatch)
- Only **one** static qword in the image points at `desc_va` (optional `bin_search` of the 8-byte VA) — usually the table slot itself
- Peek `desc_va` bytes: often uninitialized on disk → **expected**, not a failure

Optional live check (debugger attached or external RPM):

```text
name = cstr(*(desc_va + 0x8))   # should be "Raycast" when initialized
fn   = *(desc_va + 0x80)        # should be executable code
```

If live name/fn work, set `RaycastBoundFn` to the working offset (`0x80` preferred among `0x78/0x80/0x88/...`).

### Step 5 — write offsets

Update only:

```cpp
namespace Offsets::WorldRoot {
    inline constexpr uintptr_t RaycastBoundDesc = /* desc_rva */;
    inline constexpr uintptr_t RaycastBoundFn   = 0x80; // or live-validated
}
```

Do not change unrelated offsets “just in case”.

---

## Path C — if static tables moved (fallback)

If after an update Steps 2–3 fail:

1. Re-check exact strings and that the IDB matches the running build.
2. Widen the backward scan / relax the high-RVA threshold slightly; re-require `Raycast` + `FindPartOnRay` anchors.
3. **Runtime scan** (most reliable fallback):
   - In the live `RobloxPlayerBeta.exe` module, walk candidate desc VAs (from any remaining pointer tables, or known desc region).
   - Accept VA where `read_string(*(va+8)) == "Raycast"` and `*(va+0x80)` points to RX code.
   - `RaycastBoundDesc = va - module_base`.

Do not ship an offset that was only “nearby” a wrong xref (e.g. reflection dumps that store `"Raycast"` next to `"WorldRoot"` without a BoundFuncDesc pointer).

---

## Anti-patterns (common AI mistakes)

| Mistake | Why it fails |
|---|---|
| `xref("Raycast") - 8` | Hits reflection / name lists, not BoundFuncDesc |
| Require static `name` at `desc+8` | Empty in file |
| Take first pointer to `"Raycast"` | Often class/method dump, not desc |
| Hardcode old RVA and “search near it” | Region moves every update |
| Confuse `FindPartOnRay` desc with `Raycast` | Different indices in the same tables |
| Change `RaycastBoundFn` without live proof | Keep `0x80` unless validated |

---

## Minimal `py_eval` checklist (copy for the model)

```text
1. imagebase
2. exact strings: WorldRoot, Raycast, FindPartOnRay
3. longest (WorldRoot, MethodName)* table containing Raycast + FindPartOnRay
4. idx = index of Raycast
5. consecutive high-.data pointer run before FindPartOnRay strings, length == methods
6. desc_rva = table[idx] - imagebase
7. RaycastBoundFn = 0x80 (live-confirm if possible)
8. print Offsets.h lines only when steps 3–6 succeeded
```
