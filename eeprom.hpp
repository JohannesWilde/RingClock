// For the Arduino IDE I can't specify additional include paths and
// I did not want to create a public library but instead have the code
// directly in the local repo - so assume helpers is added as a submodule
// in this directory.
#include "helpers/crc16.hpp"

// todo: Make helpers a submodule of this submodule and adapt all includes of helpers... Either directly or via this...

#include <avr/eeprom.h>

#include <stdint.h>

#ifndef EEPROM_HELPERS_HPP
#define EEPROM_HELPERS_HPP

namespace Eeprom
{

typedef size_t Address;

template <typename BACKUP_DATA, Address offset>
constexpr bool fitsInEeprom()
{
    return (E2END >= (offset + sizeof(BACKUP_DATA) + 2 /* CRC */ - 1 /* index */)); // make this a template constexpr method...
}


void writeWithCrc(void const * const data, size_t const byteCount, Address const eepromAddress)
{
    Crc16Ibm3740 crc;
    crc.process(static_cast<uint8_t const *>(data), byteCount);
    uint16_t const crcValue = crc.get();

    eeprom_write_block(data, (void *)(eepromAddress), byteCount);
    eeprom_write_block(&crcValue, (void *)(eepromAddress + byteCount), 2);
}


bool readWithCrc(void * const data, size_t const byteCount, Address const eepromAddress)
{
    uint16_t crcValue = 0xffff;

    eeprom_read_block(data, (void const *)(eepromAddress), byteCount);
    eeprom_read_block(&crcValue, (void const *)(eepromAddress + byteCount), 2);

    Crc16Ibm3740 crc;
    crc.process(static_cast<uint8_t const *>(data), byteCount);

    return (crc.get() == crcValue);
}


} // namespace Eeprom

#endif // EEPROM_HELPERS_HPP
