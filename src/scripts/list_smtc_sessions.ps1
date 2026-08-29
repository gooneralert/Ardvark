Add-Type -AssemblyName System.Runtime.WindowsRuntime
$asTaskGeneric = ([System.WindowsRuntimeSystemExtensions].GetMethods() | Where-Object { $_.Name -eq 'AsTask' -and $_.GetParameters().Count -eq 1 -and $_.GetParameters()[0].ParameterType.Name -eq 'IAsyncOperation`1' })[0]
function Await($op, $t) { $m = $asTaskGeneric.MakeGenericMethod($t); $task = $m.Invoke($null, @($op)); $task.Wait(-1) | Out-Null; $task.Result }
[Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager,Windows.Media.Control,ContentType=WindowsRuntime] | Out-Null
$mgr = Await ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager]::RequestAsync()) ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager])
try { Write-Host ('Current session AUMID: ' + $mgr.GetCurrentSession().SourceAppUserModelId) } catch { Write-Host ('GetCurrentSession failed: ' + $_.Exception.Message) }
$sessions = $mgr.GetSessions()
$sessions = @($mgr.GetSessions())
Write-Host ('Total sessions: ' + $sessions.Count)
$i = 0
foreach ($s in $sessions) {
  Write-Host ('--- Session ' + $i + ' ---')
  Write-Host ('AUMID: ' + $s.SourceAppUserModelId)
  try {
    $p = Await ($s.TryGetMediaPropertiesAsync()) ([Windows.Media.Control.GlobalSystemMediaTransportControlsSessionMediaProperties])
    Write-Host ('Title:  ' + $p.Title)
    Write-Host ('Artist: ' + $p.Artist)
    Write-Host ('Album:  ' + $p.AlbumTitle)
    try { Write-Host ('Thumbnail present: ' + ($null -ne $p.Thumbnail)) } catch { Write-Host 'Thumbnail: error' }
  } catch { Write-Host ('MediaProperties failed: ' + $_.Exception.Message) }
  $pi = $s.GetPlaybackInfo()
  Write-Host ('PlaybackStatus: ' + $pi.PlaybackStatus)
  Write-Host ('Controls: play=' + $pi.IsPlayEnabled + ' pause=' + $pi.IsPauseEnabled + ' next=' + $pi.IsNextEnabled + ' prev=' + $pi.IsPreviousEnabled + ' shuffle=' + $pi.IsShuffleEnabled + ' repeat=' + $pi.IsRepeatEnabled)
  try {
    $tl = $pi.GetTimelineProperties()
    Write-Host ('Timeline pos=' + $tl.Position + ' end=' + $tl.EndTime + ' lastUpdated=' + $tl.LastUpdatedTime)
  } catch { Write-Host ('Timeline unavailable: ' + $_.Exception.Message) }
}
