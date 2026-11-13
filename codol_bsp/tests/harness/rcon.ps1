param([Parameter(Mandatory)][string]$Cmd, [int]$Port = 17733, [int]$WaitMs = 1500, [string]$Scratch = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest')
# one rcon command to the test server on loopback; prints the answer
$pw = (Get-Content "$Scratch\rcon.txt" -Raw).Trim()
$u = New-Object System.Net.Sockets.UdpClient
$u.Client.ReceiveTimeout = $WaitMs
$u.Connect('127.0.0.1', $Port)
$b = [byte[]](255,255,255,255) + [Text.Encoding]::ASCII.GetBytes("rcon `"$pw`" $Cmd")
[void]$u.Send($b, $b.Length)
$ep = New-Object System.Net.IPEndPoint([System.Net.IPAddress]::Any, 0)
$out = ''
try { while ($true) { $r = $u.Receive([ref]$ep); $out += [Text.Encoding]::ASCII.GetString($r, 4, $r.Length - 4) } } catch {}
$u.Close()
$out
