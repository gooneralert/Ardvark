import ctypes
import ctypes.wintypes
import struct
import threading
import time
from datetime import datetime

ProcessQueryInformation = 0x0400
ProcessVmOperation = 0x0008
ProcessVmRead = 0x0010
ProcessVmWrite = 0x0020
ProcessDupHandle = 0x0040
ProcessAccessRights = (
    ProcessQueryInformation | ProcessVmOperation
    | ProcessVmRead | ProcessVmWrite | ProcessDupHandle
)
MemCommit = 0x1000
MemReserve = 0x2000
MemRelease = 0x8000
PageReadWrite = 0x04
PageExecuteReadWrite = 0x40
TokenAllAccess = 0xF01FF
SePrivilegeEnabled = 0x02
DuplicateSameAccess = 0x2
Th32csSnapProcess = 0x2
ErrorNotAllAssigned = 1300
MaxNtStatusSuccess = 0x7FFFFFFF

TargetProcess = "RobloxPlayerBeta.exe"
SeDebugPrivilege = "SeDebugPrivilege"
IoCompletionTypeName = "IoCompletion"
HandleScanStart = 4
HandleScanEnd = 8192
HandleScanStep = 4
ObjectTypeInfoBufferSize = 10000
TpDirectSize = 72
TpDirectCallbackOffset = 56
PrintOffset = 0x92C340
MaxUserAddress = 0x7FFFFFFFFFFF
CaveScanChunk = 0x100000

RbxPrint = 0
RbxInfo = 1
RbxWarning = 2
RbxError = 3

LevelMap = {
    RbxPrint: 0,
    RbxInfo: 1,
    RbxWarning: 2,
    RbxError: 3,
}

HWND_TOPMOST = -1
SWP_NOSIZE = 0x0001
SWP_NOMOVE = 0x0002
SWP_NOACTIVATE = 0x0010
TOPMOST_FLAGS = SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE

Kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
Ntdll = ctypes.WinDLL("ntdll")
Advapi32 = ctypes.WinDLL("advapi32", use_last_error=True)

Handle = ctypes.wintypes.HANDLE
Dword = ctypes.wintypes.DWORD
Bool_ = ctypes.wintypes.BOOL
SizeT = ctypes.c_size_t


class LUID(ctypes.Structure):
    _fields_ = [("LowPart", ctypes.c_ulong), ("HighPart", ctypes.c_long)]


class LUID_AND_ATTRIBUTES(ctypes.Structure):
    _fields_ = [("Luid", LUID), ("Attributes", ctypes.c_ulong)]


class TOKEN_PRIVILEGES(ctypes.Structure):
    _fields_ = [
        ("PrivilegeCount", ctypes.c_ulong),
        ("Privileges", LUID_AND_ATTRIBUTES * 1),
    ]


class PROCESSENTRY32W(ctypes.Structure):
    _fields_ = [
        ("dwSize", Dword),
        ("cntUsage", Dword),
        ("th32ProcessID", Dword),
        ("th32DefaultHeapID", SizeT),
        ("th32ModuleID", Dword),
        ("cntThreads", Dword),
        ("th32ParentProcessID", Dword),
        ("pcPriClassBase", ctypes.c_long),
        ("dwFlags", Dword),
        ("szExeFile", ctypes.c_wchar * 260),
    ]


class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("BaseAddress", ctypes.c_void_p),
        ("AllocationBase", ctypes.c_void_p),
        ("AllocationProtect", Dword),
        ("RegionSize", SizeT),
        ("State", Dword),
        ("Protect", Dword),
        ("Type", Dword),
    ]


Kernel32.GetCurrentProcess.restype = Handle
Kernel32.OpenProcess.argtypes = [Dword, Bool_, Dword]
Kernel32.OpenProcess.restype = Handle
Kernel32.CreateToolhelp32Snapshot.argtypes = [Dword, Dword]
Kernel32.CreateToolhelp32Snapshot.restype = Handle
Kernel32.Process32FirstW.argtypes = [Handle, ctypes.POINTER(PROCESSENTRY32W)]
Kernel32.Process32FirstW.restype = Bool_
Kernel32.Process32NextW.argtypes = [Handle, ctypes.POINTER(PROCESSENTRY32W)]
Kernel32.Process32NextW.restype = Bool_
Kernel32.CloseHandle.argtypes = [Handle]
Kernel32.CloseHandle.restype = Bool_
Kernel32.VirtualAllocEx.argtypes = [Handle, ctypes.c_void_p, SizeT, Dword, Dword]
Kernel32.VirtualAllocEx.restype = ctypes.c_void_p
Kernel32.VirtualFreeEx.argtypes = [Handle, ctypes.c_void_p, SizeT, Dword]
Kernel32.VirtualFreeEx.restype = Bool_
Kernel32.VirtualQueryEx.argtypes = [Handle, ctypes.c_void_p, ctypes.POINTER(MEMORY_BASIC_INFORMATION), SizeT]
Kernel32.VirtualQueryEx.restype = SizeT
Kernel32.ReadProcessMemory.argtypes = [Handle, ctypes.c_void_p, ctypes.c_void_p, SizeT, ctypes.POINTER(SizeT)]
Kernel32.ReadProcessMemory.restype = Bool_
Kernel32.WriteProcessMemory.argtypes = [Handle, ctypes.c_void_p, ctypes.c_void_p, SizeT, ctypes.POINTER(SizeT)]
Kernel32.WriteProcessMemory.restype = Bool_
Kernel32.GetConsoleWindow.argtypes = []
Kernel32.GetConsoleWindow.restype = Handle
User32 = ctypes.WinDLL("user32")
User32.SetWindowPos.argtypes = [Handle, Handle, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_uint]
User32.SetWindowPos.restype = Bool_
User32.GetForegroundWindow.argtypes = []
User32.GetForegroundWindow.restype = Handle
User32.IsWindow.argtypes = [Handle]
User32.IsWindow.restype = Bool_
User32.IsWindowVisible.argtypes = [Handle]
User32.IsWindowVisible.restype = Bool_
User32.GetWindowTextW.argtypes = [Handle, ctypes.c_wchar_p, ctypes.c_int]
User32.GetWindowTextW.restype = ctypes.c_int
WNDENUMPROC = ctypes.WINFUNCTYPE(Bool_, Handle, ctypes.wintypes.LPARAM)
User32.EnumWindows.argtypes = [WNDENUMPROC, ctypes.wintypes.LPARAM]
User32.EnumWindows.restype = Bool_

Ntdll.NtQueryObject.argtypes = [Handle, ctypes.c_uint, ctypes.c_void_p, ctypes.c_ulong, ctypes.POINTER(ctypes.c_ulong)]
Ntdll.NtQueryObject.restype = ctypes.c_long
Ntdll.ZwSetIoCompletion.argtypes = [Handle, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_long, SizeT]
Ntdll.ZwSetIoCompletion.restype = ctypes.c_long

Advapi32.OpenProcessToken.argtypes = [Handle, Dword, ctypes.POINTER(Handle)]
Advapi32.OpenProcessToken.restype = Bool_
Advapi32.LookupPrivilegeValueW.argtypes = [ctypes.c_wchar_p, ctypes.c_wchar_p, ctypes.POINTER(LUID)]
Advapi32.LookupPrivilegeValueW.restype = Bool_
Advapi32.AdjustTokenPrivileges.argtypes = [Handle, Bool_, ctypes.POINTER(TOKEN_PRIVILEGES), Dword, ctypes.c_void_p, ctypes.c_void_p]
Advapi32.AdjustTokenPrivileges.restype = Bool_


def CurrentProcess():
    return Handle(Kernel32.GetCurrentProcess())


def EnablePrivilege(PrivilegeName, Attributes):
    Token = Handle()
    if not Advapi32.OpenProcessToken(CurrentProcess(), TokenAllAccess, ctypes.byref(Token)):
        return False
    try:
        PrivilegeLuid = LUID()
        if not Advapi32.LookupPrivilegeValueW(None, PrivilegeName, ctypes.byref(PrivilegeLuid)):
            return False
        Privileges = TOKEN_PRIVILEGES()
        Privileges.PrivilegeCount = 1
        Privileges.Privileges[0].Luid = PrivilegeLuid
        Privileges.Privileges[0].Attributes = Attributes
        Result = Advapi32.AdjustTokenPrivileges(
            Token, False, ctypes.byref(Privileges),
            ctypes.sizeof(TOKEN_PRIVILEGES), None, None
        )
        return bool(Result) and ctypes.get_last_error() != ErrorNotAllAssigned
    finally:
        Kernel32.CloseHandle(Token)


def FindProcessPid(TargetName):
    Snapshot = Kernel32.CreateToolhelp32Snapshot(Th32csSnapProcess, 0)
    if not Snapshot:
        return 0
    try:
        Entry = PROCESSENTRY32W()
        Entry.dwSize = ctypes.sizeof(PROCESSENTRY32W)
        LowerTarget = TargetName.lower()
        if Kernel32.Process32FirstW(Snapshot, ctypes.byref(Entry)):
            while True:
                if Entry.szExeFile.lower() == LowerTarget:
                    return Entry.th32ProcessID
                if not Kernel32.Process32NextW(Snapshot, ctypes.byref(Entry)):
                    break
    finally:
        Kernel32.CloseHandle(Snapshot)
    return 0


def BuildShellcode(Message, RbxLevel):
    ModuleHandleAddress = ctypes.cast(Kernel32.GetModuleHandleA, ctypes.c_void_p).value
    RawLevel = LevelMap.get(RbxLevel, LevelMap[RbxInfo])
    MessageBytes = Message.encode("utf-8") + b"\x00"
    Code = bytearray()
    Code += b"\x48\x83\xEC\x28"
    Code += b"\x33\xC9"
    Code += b"\x48\xB8" + struct.pack("<Q", ModuleHandleAddress)
    Code += b"\xFF\xD0"
    Code += b"\x48\x05" + struct.pack("<I", PrintOffset)  # fuhhh bump dis offset when roblox updates son
    Code += b"\x49\x89\xC2"
    Code += b"\xB9" + struct.pack("<I", RawLevel)
    Code += b"\x48\x8D\x15\x08\x00\x00\x00"
    Code += b"\x41\xFF\xD2"
    Code += b"\x48\x83\xC4\x28"
    Code += b"\xC3"
    Code += MessageBytes
    return bytes(Code)


def FindIoCompletionHandle(ProcessHandle):
    TypeInfo = (ctypes.c_ubyte * ObjectTypeInfoBufferSize)()
    TypeInfoBase = ctypes.addressof(TypeInfo)
    for RawHandle in range(HandleScanStart, HandleScanEnd, HandleScanStep):
        DupHandle = Handle()
        if not Kernel32.DuplicateHandle(
            ProcessHandle, Handle(RawHandle), CurrentProcess(),
            ctypes.byref(DupHandle), 0, False, DuplicateSameAccess
        ):
            continue
        Status = Ntdll.NtQueryObject(DupHandle, 2, TypeInfo, ObjectTypeInfoBufferSize, None)
        Matched = False
        if 0 <= Status <= MaxNtStatusSuccess:
            NamePointer = struct.unpack_from("<Q", TypeInfo, 8)[0]
            NameLength = struct.unpack_from("<H", TypeInfo, 0)[0]
            NameOffset = NamePointer - TypeInfoBase
            if NamePointer and NameLength >= 2 and 0 <= NameOffset <= ObjectTypeInfoBufferSize - NameLength:
                TypeName = bytes(TypeInfo[NameOffset:NameOffset + NameLength]).decode("utf-16-le", errors="ignore")
                Matched = TypeName == IoCompletionTypeName
        if Matched:
            return DupHandle.value
        Kernel32.CloseHandle(DupHandle)
    return None


def FindCodeCave(ProcessHandle, CaveSize):
    Needle = b"\x00" * CaveSize
    Overlap = CaveSize - 1
    Mbi = MEMORY_BASIC_INFORMATION()
    Address = 0
    while Address < MaxUserAddress:
        if not Kernel32.VirtualQueryEx(ProcessHandle, ctypes.c_void_p(Address), ctypes.byref(Mbi), ctypes.sizeof(Mbi)):
            break
        Base = Mbi.BaseAddress or 0
        RegionSize = Mbi.RegionSize
        CommittedReadWrite = Mbi.State == MemCommit and Mbi.Protect == PageReadWrite
        if CommittedReadWrite and RegionSize >= CaveSize:
            Offset = 0
            while Offset + CaveSize <= RegionSize:
                ChunkSize = min(CaveScanChunk, RegionSize - Offset)
                Buffer = (ctypes.c_ubyte * ChunkSize)()
                BytesRead = SizeT()
                if not Kernel32.ReadProcessMemory(
                    ProcessHandle, ctypes.c_void_p(Base + Offset),
                    Buffer, ChunkSize, ctypes.byref(BytesRead)
                ) or BytesRead.value < CaveSize:
                    break
                Hit = bytes(Buffer[:BytesRead.value]).find(Needle)
                if Hit != -1:
                    return Base + Offset + Hit
                NextOffset = Offset + max(ChunkSize - Overlap, 1)
                if NextOffset <= Offset:
                    break
                Offset = NextOffset
        NextAddress = Base + RegionSize
        if NextAddress <= Address:
            break
        Address = NextAddress
    return None


def WriteRemoteMemory(ProcessHandle, Address, Data):
    Buffer = (ctypes.c_byte * len(Data)).from_buffer_copy(Data)
    BytesWritten = SizeT()
    return bool(Kernel32.WriteProcessMemory(
        ProcessHandle, ctypes.c_void_p(Address),
        Buffer, len(Data), ctypes.byref(BytesWritten)
    ))


def Inject(ProcessHandle, ShellcodeAddress):
    CompletionHandle = FindIoCompletionHandle(ProcessHandle)
    if not CompletionHandle:
        print("failed to find IoCompletion handle")
        return False
    try:
        CaveAddress = FindCodeCave(ProcessHandle, TpDirectSize)
        if not CaveAddress:
            print("failed to find suitable codecave in roblox")
            return False
        # fuhhh da threadpool calls whatever callback sits at dat offset cuhhh
        Direct = bytearray(TpDirectSize)
        struct.pack_into("<Q", Direct, TpDirectCallbackOffset, ShellcodeAddress)
        if not WriteRemoteMemory(ProcessHandle, CaveAddress, bytes(Direct)):
            print(f"failed to write TP_DIRECT structure: error 0x{ctypes.get_last_error():X}")
            return False
        Status = Ntdll.ZwSetIoCompletion(CompletionHandle, ctypes.c_void_p(CaveAddress), None, 0, 0)
        if Status < 0:
            print(f"failed to set IO completion, status 0x{Status & 0xFFFFFFFF:X}")
            return False
        return True
    finally:
        Kernel32.CloseHandle(CompletionHandle)


def Printsploit(Message, Level=RbxPrint):
    HasDebugPrivilege = EnablePrivilege(SeDebugPrivilege, SePrivilegeEnabled)

    Pid = FindProcessPid(TargetProcess)
    if not Pid:
        print("target process not found")
        return 1

    ProcessHandle = Kernel32.OpenProcess(ProcessAccessRights, False, Pid)
    if not ProcessHandle:
        if not HasDebugPrivilege:
            print("process access denied. target might be elevated, try running as admin")
        else:
            print("process access denied")
        return 1

    try:
        Shellcode = BuildShellcode(Message, Level)
        RemoteCode = Kernel32.VirtualAllocEx(
            ProcessHandle, None, len(Shellcode),
            MemCommit | MemReserve, PageExecuteReadWrite
        )
        if not RemoteCode:
            print("failed to allocate shellcode memory")
            return 1
        if not WriteRemoteMemory(ProcessHandle, RemoteCode, Shellcode):
            print(f"failed to write shellcode: error 0x{ctypes.get_last_error():X}")
            Kernel32.VirtualFreeEx(ProcessHandle, RemoteCode, 0, MemRelease)
            return 1
        if not Inject(ProcessHandle, RemoteCode):
            print(f"{now} - {Message}")
            Kernel32.VirtualFreeEx(ProcessHandle, RemoteCode, 0, MemRelease)
            return 1
    finally:
        Kernel32.CloseHandle(ProcessHandle)

    print(f"{now} - {Message}")
    return 0


def FindConsoleWindow():
    Direct = Kernel32.GetConsoleWindow()
    if Direct and User32.IsWindowVisible(Direct):
        return Direct

    Found = [0]

    def Callback(Current, param):
        Length = User32.GetWindowTextW(Current, None, 0)
        if Length > 0:
            Buffer = ctypes.create_unicode_buffer(Length + 1)
            User32.GetWindowTextW(Current, Buffer, Length + 1)
            Title = Buffer.value.strip().lower()
            if Title == "c:\\windows\\py.exe" or Title.endswith("\\py.exe"):
                Found[0] = Current
                return False
        return True

    User32.EnumWindows(WNDENUMPROC(Callback), 0)
    if Found[0]:
        return Found[0]

    Foreground = User32.GetForegroundWindow()
    if Foreground and User32.IsWindowVisible(Foreground):
        return Foreground

    return 0


def KeepConsoleTopmost():
    ConsoleWindow = 0

    def Loop():
        nonlocal ConsoleWindow
        while True:
            if not User32.IsWindow(ConsoleWindow):
                ConsoleWindow = FindConsoleWindow()
            if ConsoleWindow and User32.IsWindow(ConsoleWindow):
                User32.SetWindowPos(ConsoleWindow, HWND_TOPMOST, 0, 0, 0, 0, TOPMOST_FLAGS)
            time.sleep(0.25)

    threading.Thread(target=Loop, daemon=True).start()


if __name__ == "__main__":
    KeepConsoleTopmost()
    # Printsploit("firing, normal print")
    # Printsploit("firing, error print", RbxError)
    # Printsploit("firing, info print", RbxInfo)
    # Printsploit("firing, warning print", RbxWarning)
   
    print("Printsploit - Type 'exit' to quit")
    while True:
        Custom = input(r"PS C:\Roblox> " + "\033[1;33mprint\033[0m ")
        now = datetime.now().strftime("%H:%M:%S")
        Printsploit(Custom)
        if Custom == "exit" or Custom == "end" or Custom == "quit" or Custom == "stop":
            break


    input("press enter to exit...")
