#pragma once

#include "Card.h"
#include "Interface.h"

class VidHDCard : public Card
{
public:
	VidHDCard(UINT slot) :
		Card(CT_VidHD, slot)
	{
		m_memMode = 0;
		m_SCREENCOLOR = 0;
		m_NEWVIDEO = 0;
		m_BORDERCOLOR = 0;
		m_SHADOW = 0;

		GetVideo().SetVidHD(true);
	}
	virtual ~VidHDCard(void) {}

	virtual void Destroy(void) {}
	virtual void Reset(const bool powerCycle);
	virtual void Update(const ULONG nExecutedCycles) {}
	virtual void InitializeIO(LPBYTE pCxRomPeripheral);

	static BYTE __stdcall IORead(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	void VideoIOWrite(WORD pc, WORD addr, BYTE bWrite, BYTE value, ULONG nExecutedCycles);

	bool IsSHR(void) { return (m_NEWVIDEO & 0xC0) == 0xC0; }	// 11000000 = Enable SHR(b7) | Linearize SHR video memory(b6)
	bool IsDHGRBlackAndWhite(void) { return (m_NEWVIDEO & (1 << 5)) ? true : false; }
	bool IsWriteAux(void);

	static void UpdateSHRCell(bool is640Mode, bool isColorFillMode, uint16_t addrPalette, bgra_t* pVideoAddress, uint32_t a);

	static const std::string& GetSnapshotCardName(void);
	virtual void SaveSnapshot(YamlSaveHelper& yamlSaveHelper);
	virtual bool LoadSnapshot(YamlLoadHelper& yamlLoadHelper, UINT version);

private:
	static const UINT SHR_MEMORY_END = 0x9FFF;
	UINT m_memMode;	// Only used by II/II+
	BYTE m_SCREENCOLOR;
	BYTE m_NEWVIDEO;
	BYTE m_BORDERCOLOR;
	BYTE m_SHADOW;
};
