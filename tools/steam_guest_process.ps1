param(
    [Parameter(Mandatory=$true)][ValidateSet('paths','list','start','status','stop')][string]$Action,
    [string]$RunDirectory,
    [string]$ExpectedExe = 'C:\Program Files (x86)\Steam\streaming_client.exe',
    [string]$ExpectedSha256,
    [string]$SdrConfig,
    [string]$SdrSha256,
    [switch]$RelayOnly,
    [int]$TargetPid,
    [long]$StartTicks
)
$ErrorActionPreference = 'Stop'
function Emit($value) { $value | ConvertTo-Json -Compress -Depth 5 }
function ClientArguments([string]$Invite) {
    $arguments = '--windowed --size 960x540'
    if ($SdrConfig) {
        if ($SdrConfig -match '[\r\n"<>]' -or $SdrConfig.EndsWith('\')) { throw 'Invalid public SDR path' }
        $arguments += ' --sdr_config "' + $SdrConfig + '"'
    }
    # Native CStreamingClientConfig: field 19 p2p_scope = Disabled (1).
    # The parser stops at the positional URL, so all settings must precede it.
    if ($RelayOnly) { $arguments += ' --settingsdata 980101' }
    return $arguments + ' ' + $Invite
}
function StartedIdentity($StartedProcess, [int]$TimeoutMilliseconds = 8000) {
    # Start-Process can return before its cached Path is populated. Keep its
    # original process handle/start time as the anchor, then re-query metadata.
    $StartedProcess.Refresh()
    $createdTicks = $StartedProcess.StartTime.ToUniversalTime().ToFileTimeUtc()
    if ($createdTicks -le 0) { throw 'Started client start time unavailable' }
    $timer = [Diagnostics.Stopwatch]::StartNew()
    do {
        $StartedProcess.Refresh()
        if ($StartedProcess.HasExited) { throw 'Started client exited before identity verification' }
        $fresh = Get-Process -Id $StartedProcess.Id -ErrorAction SilentlyContinue
        $path = $null
        $ticks = 0
        if ($fresh) {
            try {
                $fresh.Refresh()
                $ticks = $fresh.StartTime.ToUniversalTime().ToFileTimeUtc()
                $path = $fresh.Path
            } catch {
                # Transient module metadata lookup failure: retry, never publish it.
                $path = $null
            }
        }
        if ($ticks -gt 0 -and $ticks -ne $createdTicks) { throw 'Started client PID identity changed' }
        if (-not [string]::IsNullOrWhiteSpace($path)) {
            if ($path -ne $ExpectedExe) { throw 'Started client executable mismatch' }
            if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $ExpectedSha256) {
                throw 'Started client binary changed'
            }
            return @{ pid=$StartedProcess.Id; start_ticks=$createdTicks; exe=$path; sha256=$ExpectedSha256 }
        }
        Start-Sleep -Milliseconds 50
    } while ($timer.ElapsedMilliseconds -lt $TimeoutMilliseconds)
    throw 'Started client executable path unavailable before identity deadline'
}
function OwnedProcess {
    $p = Get-Process -Id $TargetPid -ErrorAction SilentlyContinue
    if (-not $p) { return $null }
    if ($p.Path -ne $ExpectedExe -or $p.StartTime.ToUniversalTime().ToFileTimeUtc() -ne $StartTicks) {
        throw 'Owned client identity mismatch'
    }
    if ((Get-FileHash -LiteralPath $ExpectedExe -Algorithm SHA256).Hash -ne $ExpectedSha256) {
        throw 'Owned client binary changed'
    }
    return $p
}
try {
    switch ($Action) {
        'paths' {
            Emit @{ roaming=[Environment]::GetFolderPath('ApplicationData'); local=[Environment]::GetFolderPath('LocalApplicationData') }
        }
        'list' {
            # Metadata only, no account names, titles, command lines or authentication.
            $clients = @(Get-Process -Name 'streaming_client','SteamLink' -ErrorAction SilentlyContinue)
            Emit @{ count=$clients.Count }
        }
        'start' {
            if ((@(Get-Process -Name 'streaming_client','SteamLink' -ErrorAction SilentlyContinue)).Count) { throw 'A streaming client is already running' }
            if ((Get-FileHash -LiteralPath $ExpectedExe -Algorithm SHA256).Hash -ne $ExpectedSha256) { throw 'Client binary identity changed' }
            # Private input arrives over stdin, never in this broker's command line.
            $invite = [Console]::In.ReadLine()
            $uri = [Uri]$invite
            if ($invite.Length -gt 8192 -or $invite -match '[\s"<>\\]' -or $uri.Scheme -ne 'https' -or
                $uri.Host -notin @('s.team','steamcommunity.com') -or $uri.UserInfo -or $uri.Fragment -or
                -not $uri.IsDefaultPort -or $uri.AbsolutePath -notmatch '^/remoteplay/connect/[^/]+') { throw 'Invalid private invitation' }
            if ($SdrConfig) {
                if ((Get-FileHash -LiteralPath $SdrConfig -Algorithm SHA256).Hash -ne $SdrSha256) { throw 'Public SDR configuration changed' }
            }
            $arguments = ClientArguments $invite
            $p = Start-Process -FilePath $ExpectedExe -ArgumentList $arguments -WorkingDirectory $RunDirectory -PassThru
            $identity = StartedIdentity $p
            $temporary = Join-Path $RunDirectory 'client-identity.tmp'
            [IO.File]::WriteAllText($temporary,($identity | ConvertTo-Json -Compress))
            Move-Item -LiteralPath $temporary -Destination (Join-Path $RunDirectory 'client-identity.json')
            $p.WaitForExit()
            Emit @{ exited=$true; exit_code=$p.ExitCode }
        }
        'status' {
            $p = OwnedProcess
            Emit @{ alive=($null -ne $p); owned_identity=($null -ne $p) }
        }
        'stop' {
            $p = OwnedProcess
            $forced=$false
            if ($p) {
                $null=$p.CloseMainWindow()
                if (-not $p.WaitForExit(5000)) {
                    # Re-validate immediately before terminating only our own child.
                    $p=OwnedProcess
                    if ($p) { $p.Kill(); $p.WaitForExit(5000) | Out-Null; $forced=$true }
                }
            }
            Emit @{ exited=$true; forced=$forced }
        }
    }
} catch {
    # Exception text can include invitation arguments. Never echo it.
    Emit @{ ok=$false; error='Guest process operation failed'; action=$Action }
    exit 1
}
