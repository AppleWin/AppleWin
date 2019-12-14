#pragma once

#include "Card.h"

class CardManager
{
public:
	CardManager(void)
	{
		for (UINT i=0; i<NUM_SLOTS; i++)
			m_slot[i] = new EmptyCard;
		m_aux = new EmptyCard;
		InsertAux(CT_Extended80Col);	// For Apple //e and above
	}
	~CardManager(void) {}

	void Insert(UINT slot, SS_CARDTYPE type);
	void Remove(UINT slot);
	SS_CARDTYPE QuerySlot(UINT slot) { return m_slot[slot]->QueryType(); }

	void InsertAux(SS_CARDTYPE type);
	void RemoveAux(void);
	SS_CARDTYPE QueryAux(void) { return m_aux->QueryType(); }

	//

	bool Disk2IsConditionForFullSpeed(void);
	void Disk2UpdateDriveState(UINT cycles);
	void Disk2Reset(const bool powerCycle = false);
	void Disk2SetEnhanceDisk(bool enhanceDisk);

private:
	Card* m_slot[NUM_SLOTS];
	Card* m_aux;
};
