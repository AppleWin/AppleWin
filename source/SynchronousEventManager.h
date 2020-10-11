#pragma once

class SyncEvent;

class SynchronousEventManager
{
public:
	SynchronousEventManager() : m_syncEventHead(NULL)
	{}
	~SynchronousEventManager(){}

	SyncEvent* GetHead(void) { return m_syncEventHead; }
	void SetHead(SyncEvent* head) { m_syncEventHead = head; }

	void Insert(SyncEvent* pNewEvent);
	bool Remove(int id);
	void Update(int cycles, ULONG uExecutedCycles);
	void Reset(void) { m_syncEventHead = NULL; }

private:
	SyncEvent* m_syncEventHead;
};

//

typedef int (*syncEventCB)(int id, int cycles, ULONG uExecutedCycles);

class SyncEvent
{
public:
	SyncEvent(int id, int initCycles, syncEventCB callback)
		: m_id(id),
		m_cyclesRemaining(initCycles),
		m_active(false),
		m_callback(callback),
		m_next(NULL)
	{}
	~SyncEvent(){}

	void SetCycles(int cycles)
	{
		m_cyclesRemaining = cycles;
	}

	int m_id;
	int m_cyclesRemaining;
	bool m_active;
	syncEventCB m_callback;
	SyncEvent* m_next;
};
