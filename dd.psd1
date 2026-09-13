@{
    schema = 1
    project = @{
        name = 'system-stuff'
        type = 'gui'
        'default-target' = 'app'
    }
    dependencies = @{ owner = 'dd' }
    build = @{
        'x64-windows' = @{
            debug = 'debug'
            release = 'release'
        }
    }
    targets = @(
        @{
            id = 'app'
            kind = 'gui'
            'cmake-target' = 'sys-stuff'
            'test-label' = 'system-stuff'
            'debug-path' = 'exe/sys-stuff-64d{exe}'
            'release-path' = 'exe/sys-stuff-64{exe}'
            platforms = @('x64-windows')
        }
    )
}
