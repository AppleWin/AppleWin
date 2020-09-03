// Handling of RGB videocards

enum RGB_Videocard_e
{
	Apple,
	Video7_SL7,
	LeChatMauve_EVE,
	LeChatMauve_Feline
};


void UpdateHiResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateDHiResCell (int x, int y, uint16_t addr, bgra_t *pVideoAddress, bool updateAux, bool updateMain);
int UpdateDHiRes160Cell (int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateLoResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateDLoResCell(int x, int y, uint16_t addr, bgra_t *pVideoAddress);
void UpdateText40ColorCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress, uint8_t bits, uint8_t character);
void UpdateText80ColorCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress, uint8_t bits, uint8_t character);
void UpdateHiResDuochromeCell(int x, int y, uint16_t addr, bgra_t* pVideoAddress);
void UpdateDuochromeCell(int h, int w, bgra_t* pVideoAddress, uint8_t bits, uint8_t foreground, uint8_t background);

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
void RGB_SetVideocard(RGB_Videocard_e videocard, int text_foreground = -1, int text_background = -1);
void RGB_SetRegularTextFG(int color);
void RGB_SetRegularTextBG(int color);
void RGB_EnableTextFB();
void RGB_DisableTextFB();
int RGB_IsTextFB();
