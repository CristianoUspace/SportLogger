$ErrorActionPreference = 'Stop'
# Only application assets are served; selected CSV/video files stay in the browser.
$studioAssets = @('index.html','app.js','core.js','attitude.js','media-time.js','map-view.js','docking.js','style.css')
$studioListener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
$studioListener.Start()
$studioPort = $studioListener.LocalEndpoint.Port
$studioUrl = "http://127.0.0.1:$studioPort/"
Write-Host "FoilMotion Studio: $studioUrl"
Write-Host 'Lascia aperta questa finestra durante l uso. Chiudila per arrestare Studio.'
Start-Process $studioUrl
try {
    while ($true) {
        $studioClient = $studioListener.AcceptTcpClient()
        try {
            $studioStream = $studioClient.GetStream()
            $studioStream.ReadTimeout = 2000
            $studioStream.WriteTimeout = 5000
            $studioReader = [System.IO.StreamReader]::new($studioStream, [System.Text.Encoding]::ASCII, $false, 1024, $true)
            $studioRequest = $studioReader.ReadLine()
            $studioLine = ''; $studioHeaderBytes = 0
            do { $studioLine = $studioReader.ReadLine(); $studioHeaderBytes += $studioLine.Length } while ($studioLine -and $studioHeaderBytes -lt 8192)
            $studioParts = $studioRequest -split ' '
            $studioName = if ($studioParts.Length -ge 2) { $studioParts[1].Split('?')[0].TrimStart('/') } else { '' }
            if (!$studioName) { $studioName = 'index.html' }
            $studioAllowed = $studioParts[0] -eq 'GET' -and $studioAssets -ccontains $studioName
            if ($studioAllowed) {
                $studioBytes = [System.IO.File]::ReadAllBytes((Join-Path $PSScriptRoot $studioName))
                $studioType = switch ([System.IO.Path]::GetExtension($studioName)) { '.html' {'text/html'} '.js' {'text/javascript'} '.css' {'text/css'} }
                $studioCode = '200 OK'
            } else { $studioBytes = [System.Text.Encoding]::UTF8.GetBytes('Not found'); $studioType = 'text/plain'; $studioCode = '404 Not Found' }
            $studioHeaders = "HTTP/1.1 $studioCode`r`nContent-Type: $studioType; charset=utf-8`r`nContent-Length: $($studioBytes.Length)`r`nConnection: close`r`nX-Content-Type-Options: nosniff`r`nReferrer-Policy: strict-origin-when-cross-origin`r`n`r`n"
            $studioHeadBytes = [System.Text.Encoding]::ASCII.GetBytes($studioHeaders)
            $studioStream.Write($studioHeadBytes,0,$studioHeadBytes.Length)
            $studioStream.Write($studioBytes,0,$studioBytes.Length)
        } catch { Write-Verbose $_ } finally { $studioClient.Dispose() }
    }
} finally { $studioListener.Stop() }
