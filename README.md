# fwutil
 Operate on DS firmware.

This program provides a basic set of utilities to view and edit DS firmware images. 

## Commands

### `info`: Display Info
This command prints out basic information about a firmware image. This prints the type of firmware, the location and size of each module, and some basic wireless initialization information.

### `verify`: Verify Firmware
This checks the firmware image for errors that may prevent it from working correctly. This checks the CRCs for the firmware's modules, the validity of data (i.e. modules are valid compressed data).

### `map`: Display Firmware Memory Map
This command prints out a visual memory map of the flash address space. 

### `md5`: Get Firmware MD5 Digest
Use this command to get MD5 digests of the whole firmware, as well as digests for individual modules. 

### `user`: Print User Configuration
Print out the user configuration information. This command prints the owner information, as well as the connection settings where present.

### `loc`: Get Firmware Location
Use this command to convert a RAM address to an module and an offset.

### `clean`: Clean Firmware Configuration
Cleans the firmware of the user configuration data and wireless initialization tables. This will optionally create a file with this information extracted, which can be restored using the `restore` command.

### `restore`: Restore Firmware Configuration
Use this command to restore configuration information extracted with the `clean` command.

### `db`: Dump Bytes
Dumps bytes at an address in the flash memory.

### `eb`: Enter Bytes
Enter bytes manually in the flash memory. This command will not adjust CRCs.

### `fix`: Fix Firmware Fields
Fixes some fields in the firmware that may prevent it from working correctly. As of now, this fixes CRCs for the firmware's modules, initialization tables, user configuration, and wireless connection settings.

### `compact`: Compact Firmware Modules
This command recompresses the firmware's modules. This may be used to more efficiently store data to allow for inserting a larger module.

### `export`: Export Firmware Module
Export a module from the firmware. By default this will decompress the module.

### `import`: Import Firmware Module
Import a module from a file to this firmware image.

