# compare_branches

CLI utility for getting package descriptions using [ALTRepo API](https://rdb.altlinux.org/api/), and further comparison.

### Requirements

- Qt 5
- librpm
- C++17

### Building

Change into source directory and say:

```
qmake -r
make
```

The executable and shared library will be placed in the `build` subdirectory.

### Using

```
compare_branches <branch1> <branch2>
```

*branch1* and *branch2* are the branch names of the ALT Linux distribution.
The command makes a request using method `/export/branch_binary_packages/{branch}`
As a result of execution, the command sends JSON to the standard output.

Execution result example:

```
{
    "query": [
        {
            "arch": "aarch64",
            "greater1": [
                {
                    "arch": "aarch64",
                    "buildtime": 1657802745,
                    "disttag": "p10+303432.100.3.1",
                    "epoch": 1,
                    "name": "0ad-debuginfo",
                    "release": "alt0_0_rc1.p10",
                    "source": "0ad",
                    "version": "0.1.26"
                },
                {
                    "arch": "aarch64",
                    "buildtime": 1633419531,
                    "disttag": "p10+284894.100.5.1",
                    "epoch": 0,
                    "name": "389-ds-base-libs",
                    "release": "alt1",
                    "source": "389-ds-base",
                    "version": "1.4.4.25"
                },

                ... other package descriptions

            ],
            "only1": [

                package descriptions

            ],
            "only2": [

                package descriptions

            ]
        },
        {
            "arch": "armh",
            "greater1": [ ... ],
            "only1": [ ... ],
            "only2": [ ... ]
        },

        ... other architectures

    ]
}
```

The output is divided by architecture, within each architecture there are 3 sections:

- **greater1** - All elements whose version in the *branch1* is greater than in the *branch2*
- **only1** - All elements that are in the *branch1*, but not in the *branch2*
- **only2** - All elements that are in the *branch2*, but not in the *branch1*
