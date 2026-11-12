param([Parameter(Mandatory)][string[]]$Maps, [string]$Out = 'D:\CoD\iw3xo-radiant-codol\build-editor\bsptest\c7\overlay')
# The script folder for the seventh round's test maps ( start_server.ps1 -Overlay ): round7\overlay ( c7_view.gsc, the
# dvar listener ) and for every map a level script that starts it.
$Maps = @($Maps | ForEach-Object { $_ -split '[;,]' } | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() })
New-Item -ItemType Directory -Force "$Out\maps\mp" | Out-Null
Copy-Item "$PSScriptRoot\overlay\maps\mp\c7_view.gsc" "$Out\maps\mp\c7_view.gsc" -Force
foreach ($m in $Maps) {
  $text = "// seventh round test map: the dvar listener of c7_view.gsc`r`nmain()`r`n{`r`n`tmaps\mp\_load::main();`r`n`tlevel thread maps\mp\c7_view::run();`r`n}`r`n"
  [IO.File]::WriteAllText("$Out\maps\mp\$m.gsc", $text, [Text.Encoding]::ASCII)
}
"overlay $Out for $($Maps -join ', ')"
