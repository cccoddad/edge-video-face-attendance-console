param(
    [Parameter(Mandatory = $true)]
    [ValidateScript({ Test-Path -LiteralPath $_ -PathType Leaf })]
    [string]$CsvPath
)

$ErrorActionPreference = 'Stop'
$expectedHeaders = @('工号', '时间', '事件', '相似度', '来源')
$excel = $null
$workbook = $null

try {
    $excel = New-Object -ComObject Excel.Application
    $excel.Visible = $false
    $workbook = $excel.Workbooks.Open((Resolve-Path -LiteralPath $CsvPath).Path, 0, $true)
    $headers = for ($column = 1; $column -le $expectedHeaders.Count; ++$column) {
        [string]$workbook.Worksheets.Item(1).Cells.Item(1, $column).Text
    }
    if (@(Compare-Object -ReferenceObject $expectedHeaders -DifferenceObject $headers).Count -ne 0) {
        throw ("Excel decoded unexpected CSV headers: " + ($headers -join ' | '))
    }
    Write-Output ('Excel CSV encoding verification passed: ' + ($headers -join ', '))
}
finally {
    if ($workbook) { $workbook.Close($false) }
    if ($excel) { $excel.Quit() }
    if ($workbook) { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($workbook) }
    if ($excel) { [void][Runtime.InteropServices.Marshal]::ReleaseComObject($excel) }
}
