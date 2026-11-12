param([Parameter(Mandatory)][string]$Prefix, [string[]]$Views = @(), [string[]]$Set = @(), [int]$Settle = 2500, [int]$Frames = 1, [int]$Gap = 500,
      [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest')
# With a client spawned on a devmap: ( -Set "dvar value" .. : server dvars over rcon first, ) then for every view
# "name=x y z yaw pitch" place the player ( setviewpos ) and take -Frames pictures -Gap ms apart:
# shots\<Prefix>_<name>[_<n>]_s.png. Without -Views: one picture where the player stands, named <Prefix>.
$H = (Resolve-Path "$PSScriptRoot\..").Path
# powershell -File hands an array over as one string: several views / dvars may also be given as "a=..;b=.."
$Views = @($Views | ForEach-Object { $_ -split ';' } | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() })
$Set = @($Set | ForEach-Object { $_ -split ';' } | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() })
foreach ($s in $Set) { & "$H\rcon.ps1" -Cmd "set $s" -WaitMs 300 -Scratch $Scratch | Out-Null }
function Take($name) {
  for ($i = 1; $i -le $Frames; $i++) {
    $n = if ($Frames -gt 1) { "${name}_$i" } else { $name }
    & "$H\shot.ps1" -Name $n -Scratch $Scratch
    if ($i -lt $Frames) { Start-Sleep -Milliseconds $Gap }
  }
}
if (-not $Views.Count) { Start-Sleep -Milliseconds $Settle; Take $Prefix; return }
foreach ($v in $Views) {
  $name, $spec = $v.Split('=')
  & "$H\rcon.ps1" -Cmd "dev_clientcmd 0 setviewpos $spec" -WaitMs 400 -Scratch $Scratch | Out-Null
  Start-Sleep -Milliseconds $Settle
  Take "${Prefix}_$name"
}
