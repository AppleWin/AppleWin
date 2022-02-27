#pragma once

/*  Emulador do computador TK3000 //e (Microdigital)
 *  por Fábio Belavenuto - Copyright (C) 2004
 *
 *  Adaptado do emulador Applewin por Michael O'Brien
 *
 *  Este arquivo é distribuido pela Licença Pública Geral GNU.
 *  Veja o arquivo Licenca.txt distribuido com este software.
 *
 *  ESTE SOFTWARE NÃO OFERECE NENHUMA GARANTIA
 *
 */

// Emula a CPU Z80

// Protótipos
void Z80_InitializeIO(LPBYTE pCxRomPeripheral, UINT uSlot);

// NB. These are in z80.cpp:
const std::string& Z80_GetSnapshotCardName(void);
void Z80_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper, const UINT uSlot);
bool Z80_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT uSlot, UINT version);
