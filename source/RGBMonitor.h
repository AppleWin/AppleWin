void UpdateHiResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateDHiResCell (int x, int y, uint16_t addr, bgra_t *pVideoAddress, bool updateAux, bool updateMain);
int UpdateDHiRes160Cell (int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateLoResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateDLoResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);

const UINT kNumBaseColors = 16;
typedef bgra_t (*baseColors_t)[kNumBaseColors];
void VideoInitializeOriginal(baseColors_t pBaseNtscColors);

void RGB_SetVideoMode(WORD address);
bool RGB_Is140Mode(void);
bool RGB_Is160Mode(void);
bool RGB_IsMixMode(void);
bool RGB_Is560Mode(void);
bool RGB_IsMixModeInvertBit7(void);
void RGB_ResetState(void);
void RGB_SetInvertBit7(bool state);

void RGB_SaveSnapshot(class YamlSaveHelper& yamlSaveHelper);
void RGB_LoadSnapshot(class YamlLoadHelper& yamlLoadHelper, UINT cardVersion);
