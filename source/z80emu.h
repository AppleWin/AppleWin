/*  Emulador do computador TK3000 //e (Microdigital)
 *  por F�bio Belavenuto - Copyright (C) 2004
 *
 *  Adaptado do emulador Applewin por Michael O'Brien
 *
 *  Este arquivo � distribuido pela Licen�a P�blica Geral GNU.
 *  Veja o arquivo Licenca.txt distribuido com este software.
 *
 *  ESTE SOFTWARE N�O OFERECE NENHUMA GARANTIA
 *
 */

// Emula a CPU Z80

// Prot�tipos
void ConfigureSoftcard(LPBYTE pCxRomPeripheral, UINT uSlot);

// NB. These are in z80.cpp:
void Z80_GetSnapshot(const HANDLE hFile, const UINT uZ80Slot);
void Z80_SetSnapshot(const HANDLE hFile);
