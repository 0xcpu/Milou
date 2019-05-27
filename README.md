# Milou

Milou is a toy driver, initially planned to be used for malware analysis and complement `Sysmon`.

Now, nothing is planned, it's just for fun! (probably useless).

## Contributing

Consider this as "Intentionally left blank".

## Usage

All file should be kept in `C:\Milou`. If you want to change the path, edit the `ETW` manifest, too.
`Milou` uses `ETW` to log events it subscribes to. Before installing the driver, install the `ETW` manifest:

```
wevtutil im MilouEtw.xml
```

To install the driver:
```
Miloard.exe load Milou.sys
```

To uninstall the driver:
```
Miloard unload
```

To uninstall the manifest:
```
wevtutil um MilouEtw.xml
```

View the log under `Applications and Services Logs` after enabling `Show Analytic and Debug Logs`(Adjust log size if you're going to let the driver run for a longer period than ~30 seconds as it generates many log messages).


## References

[Windows driver samples](https://github.com/microsoft/Windows-driver-samples)

## License

Copyright (c) Milou. All rights reserved.

Licensed under the [GPL v3](https://github.com/0xcpu/Milou/blob/master/LICENSE) License.
