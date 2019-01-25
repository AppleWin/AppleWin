void UpdateHiResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateDHiResCell (int x, int y, uint16_t addr, bgra_t *pVideoAddress, bool updateAux, bool updateMain);
void UpdateLoResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateDLoResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);

const UINT kNumBaseColors = 16;
typedef bgra_t (*baseColors_t)[kNumBaseColors];
void VideoInitializeOriginal(baseColors_t pBaseNtscColors);

void RGB_SetVideoMode(WORD address);
bool RGB_GetMixedMode(void);
void RGB_ResetState(void);
