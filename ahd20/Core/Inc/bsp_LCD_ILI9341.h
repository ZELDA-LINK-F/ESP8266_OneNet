#ifndef __BSP__LCD_ILI93XX_H
#define __BSP__LCD_ILI93XX_H

#ifdef USE_STDPERIPH_DRIVER
    #include "stm32f10x.h"                       // 标准库
#endif

#ifdef USE_HAL_DRIVER
    #include "stm32f1xx_hal.h"                   // HAL库
#endif



/*****************************************************************************
 ** 移植配置
****************************************************************************/
// BL_背光
#define    LCD_BL_GPIO     GPIOA
#define    LCD_BL_PIN      GPIO_PIN_15
// RD_读控制
#define    LCD_RD_GPIO     GPIOC
#define    LCD_RD_PIN      GPIO_PIN_6
// WE_写控制
#define    LCD_WE_GPIO     GPIOC
#define    LCD_WE_PIN      GPIO_PIN_7
// RS_切换数据\命令
#define    LCD_RS_GPIO     GPIOC
#define    LCD_RS_PIN      GPIO_PIN_8
// CS_片选
#define    LCD_CS_GPIO     GPIOC
#define    LCD_CS_PIN      GPIO_PIN_9

// // BL_背光
// #define LCD_BL_GPIO     GPIOA
// #define LCD_BL_PIN      GPIO_Pin_15   // 标准库格式

// // RD_读控制
// #define LCD_RD_GPIO     GPIOC
// #define LCD_RD_PIN      GPIO_Pin_6

// // WE_写控制
// #define LCD_WE_GPIO     GPIOC
// #define LCD_WE_PIN      GPIO_Pin_7

// // RS_切换数据\命令
// #define LCD_RS_GPIO     GPIOC
// #define LCD_RS_PIN      GPIO_Pin_8

// // CS_片选
// #define LCD_CS_GPIO     GPIOC
// #define LCD_CS_PIN      GPIO_Pin_9
// 屏幕参数
#define    LCD_WIDTH       240                // 屏幕宽度像素，注意：0~239
#define    LCD_HIGH        320                // 屏幕高度像素，注意：0~319
#define    LCD_DIR         6                  // 四种显示方向，0-正竖屏，3-倒竖屏，5-正横屏, 6-倒横屏



/******************************* 定义常用颜色值 *****************************/
#define      WHITE               0xFFFF       // 白色
#define      BLACK               0x0000       // 黑色 
#define      GREY                0xF7DE       // 灰色 
#define      GRAY                0X8430       // 灰色
#define      RED                 0xF800       // 红 
#define      MAGENTA             0xF81F       // 洋红色 
#define      GRED                0xFFE0       // 深红色
#define      BROWN               0XBC40       // 棕色
#define      BRRED               0XFC07       // 棕红色
#define      GREEN               0x07E0       // 绿 
#define      CYAN                0x7FFF       // 青色 
#define      YELLOW              0xFFE0       // 黄色 
#define      LIGHTGREEN          0X841F       // 浅绿色 
#define      BLUE                0x001F       // 蓝 
#define      GBLUE               0x07FF       // 浅蓝 1
#define      LIGHTBLUE           0X7D7C       // 浅蓝 2
#define      BLUE2               0x051F       // 浅蓝 3
#define      GRAYBLUE            0X5458       // 灰蓝 
#define      DARKBLUE            0X01CF       // 深蓝
#define      LGRAY               0XC618       // 浅灰色,窗体背景色
#define      LGRAYBLUE           0XA651       // 浅灰蓝色(中间层颜色)
#define      LBBLUE              0X2B12       // 浅棕蓝色(选择条目的反色)



/*****************************************************************************
 ** 声明全局函数

****************************************************************************/
// 设置
void LCD_Init(void);                                                                                   // 初始化
void LCD_SetDir(uint8_t dir);                                                                          // 设置显示方向; 0-竖屏、1-横屏
void LCD_DisplayOn(void);                                                                              // 开显示
void LCD_DisplayOff(void);                                                                             // 关显示
// 获取设置参数
uint8_t  LCD_GetDir(void);                                                                             // 获取 当前的显示方向: 0-竖屏、1-横屏
uint16_t LCD_GetWidth(void);                                                                           // 获取 宽度大小(像素); 以显示方向为准
uint16_t LCD_GetHeight(void);                                                                          // 获取 高度大小(像素)
// 基础功能
void LCD_DrawPoint(uint16_t  x, uint16_t  y, uint16_t _color);                                         // 画点函数
void LCD_Circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color);                                  // 画圆
void LCD_Line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);                     // 画线
void LCD_Rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);                // 画矩形
void LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color);                     // 填充单色
void LCD_Cross(uint16_t x, uint16_t y, uint16_t len, uint32_t fColor);                                 // 画十字线; 用于重新校准
// 扩展功能
void LCD_String(uint16_t x, uint16_t y, char *pFont, uint8_t size, uint32_t fColor, uint32_t bColor);  // 显示中英文字符串
void LCD_Image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *image) ;        // 显示图像
void LCD_DispFlush(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *pData);    // 在指定区域填充数据，适用于图片、LVGL等
// 示范
void LCD_GUI(void); 
int8_t String_Index(const char *chinese_str);   
void LCD_ShowChinese(uint8_t x, uint8_t y, uint8_t num, uint8_t size1, uint32_t fColor, uint32_t bColor);                                                                                // 绘制简单界面

#endif

