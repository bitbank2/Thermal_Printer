# MXW01 Cat Printer BLE Protocol Specification

By https://github.com/jeremy46231/MXW01-catprinter

This is a questionably-accurate and probably incomplete document describing the
MXW01's BLE protocol. Please help update it if you have more information!

## Overview

When it is on, the printer exposes one BLE service with three characteristics.

- **Main service**:`0000ae30-0000-1000-8000-00805f9b34fb` [^1]
  - **`AE01`:** Control characteristic (`0000ae01-0000-1000-8000-00805f9b34fb`)
    - Type: Write without response
    - Purpose: Sending control commands (Status Request, Print Request, Set
      Intensity, Flush, etc.).
  - **`AE02`:** Notify characteristic (`0000ae02-0000-1000-8000-00805f9b34fb`)
    - Type: Notify
    - Purpose: Receiving status responses, acknowledgments, and print completion
      notifications from the printer.
  - **`AE03`:** Data characteristic (`0000ae03-0000-1000-8000-00805f9b34fb`)
    - Type: Write without response
    - Purpose: Sending bulk image data to the printer.

[^1]:
    [A comment](https://github.com/rbaron/catprinter/blob/9ec21a8/catprinter/ble.py#L11-L13)
    in rbaron/catprinter suggests that the main `0000ae30-...` service sometimes
    shows up as `0000af30-...` instead, particularly on Macs, though that repo
    is not for the MXW01.

All messages sent to the printer begin with a `0x22 0x21` preamble.

## `AE01` Control Packet Structure

Commands sent _to_ the printer via the Control Characteristic (`AE01`) follow
this structure:

| Field           | Length (Bytes) | Value               | Description                                     |
| :-------------- | :------------- | :------------------ | :---------------------------------------------- |
| Preamble        | 2              | `0x22 0x21`         |                                                 |
| Command ID      | 1              | Command ID          | See the [Command Reference](#command-reference) |
| Fixed (unknown) | 1              | `0x00`              | Appears fixed, unknown purpose                  |
| Length (LE)     | 2              | `0x0000` - `0xFFFF` | Length of the `Payload` field, Little Endian    |
| Payload         | Variable       | Command-specific    | See the [Command Reference](#command-reference) |
| CRC8            | 1              | `0x00` - `0xFF`     | CRC checksum (see below)                        |
| Footer          | 1              | `0xFF`              |                                                 |

### CRC Calculation

- **Algorithm:** CRC-8 / DALLAS-MAXIM
- **Parameters:**
  - Polynomial: `0x07` (x^8 + x^2 + x^1 + x^0)
  - Initial Value: `0x00`
  - Reflect Input: `False`
  - Reflect Output: `False`
  - XOR Output: `0x00`
- **Scope:** Calculated **only** over the `Payload` data field.

## `AE02` Notification Packet Structure

Notifications received _from_ the printer via the Notify Characteristic (`AE02`)
generally follow the same structure as commands:

| Field       | Length (Bytes) | Value                  | Description                                                       |
| :---------- | :------------- | :--------------------- | :---------------------------------------------------------------- |
| Preamble    | 2              | `0x22 0x21`            |                                                                   |
| Command ID  | 1              | Command ID             | See the [Command Reference](#command-reference)                   |
| Unknown     | 1              | Variable byte          | Unknown. Observed as non-zero (eg, `0x03`) in some `A1` responses |
| Length (LE) | 2              | `0x0000` - `0xFFFF`    | Length of the `Payload` field, Little Endian                      |
| Payload     | Variable       | Response-specific data | See the [Command Reference](#command-reference)                   |
| Footer      | 1              | `0xFF`                 |                                                                   |

## Command Reference

| ID (Hex)                   | Name                   | Direction | Payload                                  | Description                                                |
| :------------------------- | :--------------------- | :-------- | :--------------------------------------- | :--------------------------------------------------------- |
| **Printing & Core Status** |                        |           |                                          |                                                            |
| `A1`                       | Get Status             | Send      | `0x00`                                   | Request printer status (paper, temp, battery, state)       |
| `A1`                       | Status Response        | Receive   | See below                                | Response to Get Status request                             |
| `A2`                       | Set Print Intensity    | Send      | Intensity, `0x00`-`0xFF`                 | Set printing darkness (`0x5D` is a good choice)            |
| `A9`                       | Print Request          | Send      | `line_count_le(2)`, `0x30`, `0x01`       | Initiate print: total lines, fixed 0x30, mode (0=1bpp)[^2] |
| `A9`                       | Print Response         | Receive   | Status byte (`0x00` = OK)                | Acknowledgment for the Print Request. Wait for this        |
| `AD`                       | Print Data Flush       | Send      | `0x00`                                   | Signal end of image data transfer via `AE03`               |
| `AA`                       | Print Complete         | Receive   | Unknown                                  | Physical print process finished (unknown payload)          |
| **Optional Status/Info**   |                        |           |                                          |                                                            |
| `AB`                       | Get Battery Level      | Send      | `0x00`                                   | Request battery level                                      |
| `AB`                       | Battery Level Response | Receive   | Battery byte                             | Response containing battery level                          |
| `B1`                       | Get Version            | Send      | `0x00`                                   | Request firmware version / printer type                    |
| `B1`                       | Version Response       | Receive   | `version_utf8(N)`, unknown, type byte    | Response with version string and type code                 |
| `B0`                       | Get Print Type         | Send      | `0x00`                                   | Request printer type information                           |
| `B0`                       | Print Type Response    | Receive   | Type byte (e.g., `0x01`, `0xFF`, `0x31`) | Response with type code                                    |
| `A7`                       | Query Count            | Send      | `0x00`?                                  | Unknown                                                    |
| `A7`                       | Query Count Response   | Receive   | Unknown (Often `FF FF...`)               | Response to Query Count. Unknown                           |
| **Optional Control**       |                        |           |                                          |                                                            |
| `AC`                       | Cancel Print           | Send      | `0x00`?                                  | Attempt to cancel ongoing print job                        |
| **Unknown**                |                        |           |                                          |                                                            |
| `A3`                       | Unknown                | ?         | ?                                        | Purpose unknown                                            |
| `AE`                       | Unknown                | ?         | ?                                        | Purpose unknown                                            |
| `B2`                       | Unknown                | ?         | ?                                        | Purpose unknown ("learn"?)                                 |
| `B3`                       | Unknown                | ?         | ?                                        | Purpose unknown ("sign/encryption"?)                       |

[^2]:
    I think there are other print modes, like the "HD" option in the official
    app, which I think might send 4 bits per pixel instead, but I'm not sure.

**`A1` Status Payload:** TODO: more precisely describe this

- `Payload[6]`: Status code (0=Standby, 1=Printing, etc.) if `Payload[12]` is
  OK
- `Payload[9]`: Battery level (approx)
- `Payload[10]`: Temperature (approx)
- `Payload[12]`: Overall Status Flag (0 = OK, Non-zero = Error)
- `Payload[13]` (if Flag!=0): Error Code (1/9=No Paper, 4=Overheated, 8=Low
  Battery). Requires length check

## Example Printing Sequence

This is a standard sequence that you can implement to communicate with the
printer.

1.  **Connect:** Establish BLE connection to the printer
2.  **Discover:** Find the Main Service and the Control (`AE01`), Notify
    (`AE02`), and Data (`AE03`) characteristics
3.  **Start Notifications:** Enable notifications on the Notify characteristic
    (`AE02`)
4.  **(Optional) Set Intensity:** Send `A2` command with intensity byte to
    `AE01`
5.  **Check Status:**
    - Send `A1` command (`[0x00]` payload) to `AE01`
    - Wait for `A1` notification on `AE02`
    - Parse the notification payload to ensure the printer is ready (Status Flag
      OK, no paper error, etc.). Abort if not ready
6.  **Send Print Request:**
    - Calculate `line_count` (total height in pixels of the image data buffer)
    - Send `A9` command (`[line_count_le(2), 0x30, 0x00]` payload for 1bpp) to
      `AE01`
    - Wait for `A9` notification on `AE02`
    - Check the response payload (first byte should be `0x00`). Abort if request
      rejected
7.  **Transfer Image Data:**
    - Send the prepared (and potentially padded) image byte buffer to the **Data
      Characteristic (`AE03`)**
    - Send data in chunks with a small delay (`~0.01s - 0.05s`) between chunks
      to avoid overwhelming the printer buffer
8.  **Flush Data:**
    - Send `AD` command (`[0x00]` payload) to `AE01` after the _last_ data chunk
      has been sent
9.  **Wait for Completion:**
    - Wait for the `AA` notification on `AE02`, indicating the printer has
      finished the physical print
10. **Disconnect:** Stop notifications and disconnect from the printer

## Image Data Encoding

Pretty simple, pack the bits into bytes, left to right, top to bottom, black is
1, white is 0, pad with zeroes to 4320 bytes minimum.

- **Input:** A 1-bit image (black = 1, white = 0), 384 pixels wide
- **Byte Packing:** Pixels are processed row by row. Each row of 384 pixels is
  converted into 48 bytes (`384 / 8 = 48`).
- **Bit Order:** Within each byte, the least significant bit (bit 0) corresponds
  to the leftmost pixel of the 8-pixel group. The most significant mit (bit 7)
  corresponds to the rightmost pixel.
- **Padding:** The final concatenated byte buffer containing all image rows
  might need padding with `0x00` bytes at the end if its total length is less
  than a minimum threshold (probably 4320 bytes / 90 lines).
