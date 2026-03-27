#!/usr/bin/env pwsh
# This script compiles GLSL shaders to SPIR-V using glslangValidator

# Path to glslangValidator (make sure it's in your PATH or set full path here)
$GLSLANG_PATH = "glslangValidator"

# Path to shader source folder
$SHADER_FOLDER = "shaders"

# Output folder
$OUTPUT_FOLDER = "assets/spv"

# Create output folder if it doesn't exist
New-Item -ItemType Directory -Force -Path $OUTPUT_FOLDER | Out-Null

# Resolve full paths
$SHADER_FOLDER_FULL = (Resolve-Path $SHADER_FOLDER).Path
$OUTPUT_FOLDER_FULL = (Resolve-Path $OUTPUT_FOLDER).Path

# Find all shader files
$shaderExtensions = @("*.vert", "*.frag", "*.comp", "*.geom", "*.tesc", "*.tese")
$shaderFiles = Get-ChildItem -Path $SHADER_FOLDER_FULL -Recurse -File -Include $shaderExtensions

foreach ($inputFile in $shaderFiles) {
    # Get relative path
    $relativePath = $inputFile.FullName.Substring($SHADER_FOLDER_FULL.Length + 1)
    $relativeDir = Split-Path $relativePath -Parent

    if ($relativeDir) {
        $targetDir = Join-Path $OUTPUT_FOLDER_FULL $relativeDir
    } else {
        $targetDir = $OUTPUT_FOLDER_FULL
    }

    New-Item -ItemType Directory -Force -Path $targetDir | Out-Null

    # Get base filename and extension
    $baseName = $inputFile.BaseName
    $ext = $inputFile.Extension.TrimStart('.')

    $outputFile = Join-Path $targetDir "${baseName}_${ext}.spv"
    $outputRelative = $outputFile.Substring($OUTPUT_FOLDER_FULL.Length + 1)

    Write-Host "Compiling " -NoNewline -ForegroundColor Blue
    Write-Host $relativePath -NoNewline -ForegroundColor Yellow
    Write-Host " -> " -NoNewline -ForegroundColor Blue
    Write-Host $outputRelative -ForegroundColor Green
    Write-Host "..." -ForegroundColor Blue

    & $GLSLANG_PATH -V $inputFile.FullName -o $outputFile

    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to compile $relativePath" -ForegroundColor Red
    } else {
        Write-Host "Successfully compiled $relativePath" -ForegroundColor Green
    }
}

Write-Host "Done." -ForegroundColor Cyan
