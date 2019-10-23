elf2dfuse
=========

This tool is a possible aid for STM32 developers who want to generate a DfuSe image directly from a STM32 ELF object file.

I have made changes on the original from majbthrd/elf2dfuse.

The user must specify a list of VID:PID of DFU targets in a file, the utility will generate dfu files per VID:PID pair.

The file names will be based on the template of the second parameter. If the template does not have a file extension, a default file extension will be added (.dfu). The DFU files generated will hae a name in the form: <file name prefix>_VID_PID.<file extension>

The VID:PID file specification is a simple line seperated text file, each line has a pair VID:PID values. The format of VID, or PID is a 16 bit hexadecimal (0483 for VID of STMicroelectronics, DF11 for PID of dfu device from STMicroelectronics).

## Limitations

DFU (and DfuSe) images must encode the VID:PID of the target being programmed.  The file specifying the VID:PID to be 0483:df11 has to be supplied on calling the utility.

## Sample Usage

```
elf2dfuse myapp.elf <file name prefix>.dfu vidpid.txt
```

