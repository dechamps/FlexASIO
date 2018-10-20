$ErrorActionPreference = 'Stop'

# See https://github.com/microsoft/vswhere
$ProgramFiles = $dir = (${env:ProgramFiles(x86)}, ${env:ProgramFiles} -ne $null)[0]
$InstallationPath = & "$($ProgramFiles)\Microsoft Visual Studio\Installer\vswhere.exe" -requires Microsoft.Component.MSBuild -format value -property installationPath

$RedistVersion = @(Get-ChildItem "$($InstallationPath)\VC\Redist\MSVC")[0]

&{
    Write-Output '// FILE AUTOGENERATED BY find_visual_studio.ps1'
    Write-Output "#define VISUAL_STUDIO_INSTALLATION_PATH ""$($InstallationPath)"""
    Write-Output "#define VISUAL_STUDIO_REDIST_PATH ""$($InstallationPath)\VC\Redist\MSVC\$($RedistVersion)"""
} |
# Powershell by default outputs UTF-16, which confuses Inno Setup Preprocessor.
# If we use UTF-8, it still insists in outputting a BOM, which still confuses ISPP.
Out-File -Encoding ASCII build\visual_studio.h