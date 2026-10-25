@{
    schema = 1
    project = @{
        name = 'sys-mate'
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
            'cmake-target' = 'sys-mate'
            'test-label' = 'sys-mate'
            'debug-path' = 'exe/sys-mate-64d{exe}'
            'release-path' = 'exe/sys-mate-64{exe}'
            platforms = @('x64-windows')
        }
    )
}
