# Milou - driver suite for malware analysis

Milou is a driver suite that combines multiple drivers in order to help during the malware analysis
process.

## Contributing

Here should be a short intro on how to build the solution and coding guidelines

## Usage

How to install the ETW, load drivers, unload, view in Event viewer...

For now:
```
wevtutil im MilouEtw.xml
Miloard.exe load Milou.sys
Miloard unload
wevtutil um MilouEtw.xml
```

View the log under `Applications and Services Logs` after enabling `Show Analytic and Debug Logs`.
Should add a picture here..

## Feedback

List of emails, twitter handles and how to get in touch...

## References

Links to projects that were helpful, books

## License

Copyright (c) Milou Project. All rights reserved.

Licensed under the [GPL v3](https://github.com/0xcpu/Milou/blob/master/LICENSE) License.
