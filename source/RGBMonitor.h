// Handling of RGB videocards

typedef enum
{
	RGB_Apple,
	RGB_Video7_SL7,
	RGB_LeChatMauve_EVE,
	RGB_LeChatMauve_Feline
} RGB_Videocard_e;


void UpdateHiResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateDHiResCell (int x, int y, uint16_t addr, bgra_t *pVideoAddress, bool updateAux, bool updateMain);
int UpdateDHiRes160Cell (int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateLoResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateDLoResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateText40DuochromeCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress, uint8_t bits);
void UpdateHiResDuochromeCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress);
void UpdateDuochromeCell(int h, bgra_t* pVideoAddress, uint8_t bits, uint8_t foreground, uint8_t background);

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

RGB_Videocard_e RGB_GetVideocard(void);
void RGB_SetVideocard(RGB_Videocard_e videocard);
void RGB_SetRegularTextFG(int color);
void RGB_SetRegularTextBG(int color);
void RGB_EnableTextFB();
void RGB_DisableTextFB();
int RGB_IsTextFB();
