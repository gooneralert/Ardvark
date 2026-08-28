$f = "c:\Users\blake\Downloads\Ardvark-new\src\src\gui\glass.cpp"
$t = [System.IO.File]::ReadAllText($f)

$old = "            g_pUpdateVD    = (fn_update_vd)GetProcAddress(dwmapi, MAKEINTRESOURCEA(164));`n            Cheat::Console::Log(Cheat::Console::Color::Gray, \"[glass] dcomp ordinals create=%p update=%p\","
$new = "            g_pUpdateVD    = (fn_update_vd)GetProcAddress(dwmapi, MAKEINTRESOURCEA(164));`n`n            // The reference branches on build: >= 20000 uses the MultiWindow 8-arg update,`n            // older Windows uses the 7-arg VirtualDesktop update.`n            g_useVD = false;`n            HMODULE ntdll = GetModuleHandleW(L\"ntdll.dll\");`n            if (ntdll)`n            {`n                typedef LONG(WINAPI* RtlGetVersionFn)(PRTL_OSVERSIONINFOW);`n                auto rtl = (RtlGetVersionFn)GetProcAddress(ntdll, \"RtlGetVersion\");`n                if (rtl)`n                {`n                    RTL_OSVERSIONINFOW vi{};`n                    vi.dwOSVersionInfoSize = sizeof(vi);`n                    if (rtl(&vi) == 0 && vi.dwBuildNumber < 20000)`n                        g_useVD = true;`n                }`n            }`n            Cheat::Console::Log(Cheat::Console::Color::Gray, \"[glass] dcomp ordinals create=%p update=%p (useVD=%d)\","
if ($t.Contains($old)) {
  $t = $t.Replace($old, $new)
  [System.IO.File]::WriteAllText($f, $t)
  Write-Output "OK"
} else {
  Write-Output "NOT FOUND"
}
