## For Windows

This is a test for src: DynamicPlugin, you shell check the symbol by DumpBin.exe by executing-this

```
dumpbin.exe -exports .\DLLDemo.dll
```

```
Microsoft (R) COFF/PE Dumper Version 14.36.32532.0
Copyright (C) Microsoft Corporation.  All rights reserved.


Dump of file .\DLLDemo.dll

File Type: DLL

  Section contains the following exports for DLLDemo.dll

    00000000 characteristics
    FFFFFFFF time date stamp
        0.00 version
           1 ordinal base
           1 number of functions
           1 number of names

    ordinal hint RVA      name

          1    0 00011244 add = @ILT+575(add)

  Summary

        1000 .00cfg
        1000 .data
        1000 .idata
        1000 .msvcjmc
        3000 .pdata
        3000 .rdata
        1000 .reloc
        1000 .rsrc
        8000 .text
       10000 .textbss
```

