--[[
  jewsploit LuaVM — maximal Phase-1 stress test
  Covers: print/warn/identifyexecutor, task/wait/spawn, coroutine,
          game/workspace bridge, Instance metamethods, Players/Character/Humanoid/Parts,
          table/string/math/utf8, nested yields, __eq/__tostring, GetService tree walk.
  Expect: many print lines + final "PASS" or "FAIL: ..."
]]

local PASS, FAIL = 0, 0
local notes = {}

local function ok(name, cond, detail)
	if cond then
		PASS = PASS + 1
		print(string.format("[OK]   %s", name))
	else
		FAIL = FAIL + 1
		local msg = detail and (name .. " — " .. tostring(detail)) or name
		table.insert(notes, msg)
		warn(string.format("[FAIL] %s", msg))
	end
end

local function pcall_ok(name, fn)
	local okv, a, b, c = pcall(fn)
	if not okv then
		ok(name, false, a)
		return nil
	end
	return a, b, c
end

--------------------------------------------------------------------------
-- 1) sandbox / identity / libs
--------------------------------------------------------------------------
do
	local name, ver = identifyexecutor()
	ok("identifyexecutor name", type(name) == "string" and #name > 0, name)
	ok("identifyexecutor ver", type(ver) == "string", ver)

	ok("print exists", type(print) == "function")
	ok("warn exists", type(warn) == "function")
	ok("wait exists", type(wait) == "function")
	ok("spawn exists", type(spawn) == "function")
	ok("task.wait", type(task) == "table" and type(task.wait) == "function")
	ok("task.spawn", type(task) == "table" and type(task.spawn) == "function")

	ok("dofile removed", dofile == nil)
	ok("loadfile removed", loadfile == nil)

	ok("math", type(math) == "table" and math.sqrt(9) == 3)
	ok("string", string.upper("ab") == "AB")
	ok("table", #{ 1, 2, 3 } == 3)
	ok("utf8", type(utf8) == "table" and utf8.len("я") == 1)
	ok("coroutine", type(coroutine) == "table")
end

--------------------------------------------------------------------------
-- 2) pure Lua stress (closures, nested tables, recursion, utf8)
--------------------------------------------------------------------------
local function deep_sum(t, depth)
	depth = depth or 0
	if depth > 40 then return 0 end
	local s = 0
	for k, v in pairs(t) do
		if type(v) == "number" then
			s = s + v
		elseif type(v) == "table" then
			s = s + deep_sum(v, depth + 1)
		elseif type(v) == "string" then
			s = s + #v
		end
		if type(k) == "number" then s = s + k * 0 end
	end
	return s
end

local tree = {}
do
	local cur = tree
	for i = 1, 12 do
		cur.n = i
		cur.s = string.rep(utf8.char(0x0416 + (i % 10)), i) -- Ж+
		cur.child = { val = i * i, nest = {} }
		cur = cur.child.nest
	end
end
ok("deep_sum tree", deep_sum(tree) > 0, deep_sum(tree))

local function fib(n, memo)
	memo = memo or {}
	if n < 2 then return n end
	if memo[n] then return memo[n] end
	memo[n] = fib(n - 1, memo) + fib(n - 2, memo)
	return memo[n]
end
ok("fib(20)", fib(20) == 6765)

local packed = {}
for i = 1, 64 do
	packed[i] = string.format("%02x", (i * 17) % 256)
end
table.sort(packed)
ok("table.sort", packed[1] <= packed[#packed])
local joined = table.concat(packed, ",")
ok("table.concat", type(joined) == "string" and #joined > 100)

local bitish = 0
for i = 0, 31 do
	bitish = bitish + (i % 2 == 0 and math.floor(2 ^ i) or 0)
end
ok("math.pow loop", bitish > 0)

--------------------------------------------------------------------------
-- 3) coroutine (Lua) + spawn/wait (VM)
--------------------------------------------------------------------------
local co_hits = 0
local co = coroutine.create(function()
	for i = 1, 5 do
		co_hits = co_hits + i
		coroutine.yield(i)
	end
	return "done"
end)
while coroutine.status(co) ~= "dead" do
	local st, v = coroutine.resume(co)
	ok("coro resume", st, v)
	if not st then break end
end
ok("coro sum", co_hits == 15, co_hits)

local spawn_flag = false
spawn(function()
	spawn_flag = true
	print("[spawn] sync body ran")
end)
ok("spawn sync", spawn_flag == true)

local task_spawn_flag = false
task.spawn(function()
	task_spawn_flag = true
end)
ok("task.spawn sync", task_spawn_flag == true)

-- nested yields across task.wait / wait
print("… yielding 3x (task.wait / wait) — keep executor open …")
local t0 = 0
task.wait(0.05)
wait(0.05)
task.wait(0.02)
ok("multi-yield survived", true)

spawn(function()
	wait(0.08)
	print("[spawn] delayed after wait")
end)

--------------------------------------------------------------------------
-- 4) game / workspace globals + DataModel
--------------------------------------------------------------------------
ok("game userdata", game ~= nil)
ok("workspace global", workspace ~= nil)

local placeId = pcall_ok("game.PlaceId", function() return game.PlaceId end)
ok("PlaceId number", type(placeId) == "number", placeId)

local gameId = pcall_ok("game.GameId", function() return game.GameId end)
ok("GameId number", type(gameId) == "number", gameId)

local jobId = pcall_ok("game.JobId", function() return game.JobId end)
ok("JobId string", type(jobId) == "string", jobId)

local gws = pcall_ok("game.Workspace", function() return game.Workspace end)
ok("game.Workspace", gws ~= nil)
if gws and workspace then
	ok("workspace == game.Workspace", workspace == gws)
end

print(string.format("tostring(game) = %s", tostring(game)))
ok("tostring game", type(tostring(game)) == "string" and #tostring(game) > 0)

--------------------------------------------------------------------------
-- 5) GetService / children / Find*
--------------------------------------------------------------------------
local Players = pcall_ok("GetService Players", function()
	return game:GetService("Players")
end)
ok("Players service", Players ~= nil)

local Lighting = pcall_ok("GetService Lighting", function()
	return game:GetService("Lighting")
end)
-- Lighting may exist; just don't crash
ok("GetService Lighting no-crash", true, Lighting and Lighting.ClassName or "nil")

local kids = pcall_ok("game:GetChildren", function() return game:GetChildren() end)
ok("GetChildren table", type(kids) == "table")
local kid_n = 0
if type(kids) == "table" then
	for i, ch in ipairs(kids) do
		kid_n = kid_n + 1
		if i <= 8 then
			print(string.format("  child[%d] %s (%s) addr=%s",
				i, tostring(ch.Name), tostring(ch.ClassName), tostring(ch.Address)))
		end
	end
end
ok("GetChildren count>0", kid_n > 0, kid_n)

local ws2 = pcall_ok("FindFirstChild Workspace", function()
	return game:FindFirstChild("Workspace")
end)
ok("FindFirstChild Workspace", ws2 ~= nil)

local dm_cls = pcall_ok("FindFirstChildOfClass", function()
	return game:FindFirstChildOfClass("Workspace")
end)
ok("FindFirstChildOfClass Workspace", dm_cls ~= nil)

ok("game:IsA DataModel", game:IsA("DataModel") == true)
ok("game:IsA Fake", game:IsA("FakeClass") == false)

local full = pcall_ok("GetFullName", function()
	if Players then return Players:GetFullName() end
	return ""
end)
ok("GetFullName", type(full) == "string", full)

--------------------------------------------------------------------------
-- 6) Players / LocalPlayer / Character / Humanoid / HRP
--------------------------------------------------------------------------
local lp = Players and pcall_ok("LocalPlayer", function() return Players.LocalPlayer end)
ok("LocalPlayer", lp ~= nil)

local plist = Players and pcall_ok("GetPlayers", function() return Players:GetPlayers() end)
ok("GetPlayers table", type(plist) == "table")
local pn = 0
if type(plist) == "table" then
	for _, pl in ipairs(plist) do
		pn = pn + 1
		local dn = pl.DisplayName
		local uid = pl.UserId
		print(string.format("  player %s dn=%s uid=%s",
			tostring(pl.Name), tostring(dn), tostring(uid)))
		ok("player Name", type(pl.Name) == "string" and #pl.Name > 0)
		ok("player UserId", type(uid) == "number")
		ok("player __eq self", pl == pl)
	end
end
ok("GetPlayers n>=1", pn >= 1, pn)

if lp then
	ok("lp ClassName Player", lp.ClassName == "Player", lp.ClassName)
	ok("lp Parent is Players", lp.Parent == Players)
	print(string.format("LocalPlayer=%s UserId=%s DisplayName=%s",
		tostring(lp.Name), tostring(lp.UserId), tostring(lp.DisplayName)))

	local char = pcall_ok("Character", function() return lp.Character end)
	ok("Character (may be nil if dead)", true, char and char.Name or "nil")

	if char then
		ok("char ClassName", type(char.ClassName) == "string")
		local hum = pcall_ok("Humanoid via Find", function()
			return char:FindFirstChildOfClass("Humanoid") or char:FindFirstChild("Humanoid")
		end)
		-- also child sugar
		local hum2 = pcall_ok("Humanoid via index", function() return char.Humanoid end)
		if hum and hum2 then
			ok("Humanoid == index", hum == hum2)
		end

		if hum then
			print(string.format("  Humanoid Health=%s/%s WalkSpeed=%s DisplayName=%s",
				tostring(hum.Health), tostring(hum.MaxHealth),
				tostring(hum.WalkSpeed), tostring(hum.DisplayName)))
			ok("Health number", type(hum.Health) == "number")
			ok("MaxHealth number", type(hum.MaxHealth) == "number")
			ok("WalkSpeed number", type(hum.WalkSpeed) == "number")
			ok("Health <= MaxHealth", hum.Health <= hum.MaxHealth + 0.01)

			local root = pcall_ok("RootPart", function() return hum.RootPart end)
			local hrp = pcall_ok("HumanoidRootPart", function()
				return char:FindFirstChild("HumanoidRootPart") or char.HumanoidRootPart
			end)
			if root and hrp then
				ok("RootPart == HRP", root == hrp)
			end

			local part = root or hrp
			if part then
				local pos = part.Position
				local size = part.Size
				ok("Position table", type(pos) == "table" and type(pos.x) == "number")
				ok("Size table", type(size) == "table" and type(size.y) == "number")
				ok("Transparency number", type(part.Transparency) == "number")
				ok("Anchored boolean", type(part.Anchored) == "boolean")
				print(string.format("  HRP pos=(%.2f, %.2f, %.2f) size=(%.2f, %.2f, %.2f) anch=%s t=%.2f",
					pos.x, pos.y, pos.z, size.x, size.y, size.z,
					tostring(part.Anchored), part.Transparency))

				-- walk character children, collect BaseParts
				local parts = {}
				for _, ch in ipairs(char:GetChildren()) do
					local cn = ch.ClassName
					if cn == "Part" or cn == "MeshPart" or cn == "BasePart" then
						table.insert(parts, ch)
					end
				end
				ok("char has parts", #parts >= 1, #parts)
				local dist_sum = 0
				for _, p in ipairs(parts) do
					local a, b = p.Position, pos
					local dx, dy, dz = a.x - b.x, a.y - b.y, a.z - b.z
					dist_sum = dist_sum + math.sqrt(dx * dx + dy * dy + dz * dz)
				end
				print(string.format("  part-distance sum from HRP: %.3f (%d parts)", dist_sum, #parts))
			end
		end
	end
end

--------------------------------------------------------------------------
-- 7) Workspace camera + shallow world scan
--------------------------------------------------------------------------
if workspace then
	local cam = pcall_ok("CurrentCamera", function() return workspace.CurrentCamera end)
	ok("CurrentCamera", cam ~= nil, cam and cam.ClassName or "nil")
	if cam then
		print(string.format("  Camera=%s Parent=%s", tostring(cam.Name), tostring(cam.Parent and cam.Parent.Name)))
		ok("Camera fullname", type(cam:GetFullName()) == "string")
	end

	local scanned = 0
	local classes = {}
	for _, ch in ipairs(workspace:GetChildren()) do
		scanned = scanned + 1
		local cn = ch.ClassName or "?"
		classes[cn] = (classes[cn] or 0) + 1
		if scanned >= 40 then break end
	end
	ok("workspace scan", scanned > 0, scanned)
	local top = {}
	for k, v in pairs(classes) do
		table.insert(top, { k = k, v = v })
	end
	table.sort(top, function(a, b) return a.v > b.v end)
	local bits = {}
	for i = 1, math.min(6, #top) do
		bits[#bits + 1] = string.format("%s=%d", top[i].k, top[i].v)
	end
	print("  workspace class hist: " .. table.concat(bits, ", "))
end

--------------------------------------------------------------------------
-- 8) Parent chain walk + Address uniqueness
--------------------------------------------------------------------------
if lp then
	local chain = {}
	local cur = lp
	for _ = 1, 16 do
		if not cur then break end
		table.insert(chain, string.format("%s@%s", tostring(cur.Name), tostring(cur.Address)))
		cur = cur.Parent
	end
	print("  parent chain: " .. table.concat(chain, " -> "))
	ok("parent chain len>=2", #chain >= 2, #chain)

	local a1 = lp.Address
	local a2 = lp.Address
	ok("Address stable", a1 == a2 and type(a1) == "number")
end

--------------------------------------------------------------------------
-- 9) heavy tostring / ipairs over players+children
--------------------------------------------------------------------------
local dump_n = 0
if Players then
	for _, pl in ipairs(Players:GetPlayers()) do
		local ch = pl.Character
		if ch then
			for _, part in ipairs(ch:GetChildren()) do
				dump_n = dump_n + 1
				local _ = tostring(part) -- __tostring path
				if dump_n >= 80 then break end
			end
		end
		if dump_n >= 80 then break end
	end
end
ok("tostring spam", dump_n >= 0, dump_n)
print(string.format("  tostring'd %d character children", dump_n))

--------------------------------------------------------------------------
-- 10) final delayed checkpoint (proves Tick resumes after long wait)
--------------------------------------------------------------------------
task.wait(0.12)
ok("post-long-wait still alive", true)

print("============================================================")
print(string.format("RESULT  PASS=%d  FAIL=%d", PASS, FAIL))
if FAIL == 0 then
	print("PASS — jewsploit LuaVM Phase-1 stress OK")
else
	warn("FAIL — details:")
	for _, n in ipairs(notes) do
		warn("  • " .. n)
	end
end
print("============================================================")
