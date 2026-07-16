/***********************************************************************************************************************************

 ********************************************************************************************************************************
 *   ��ʵ��ƽ̨��STM32F103RC + KEIL5.27 + 2.8����ʾ��_ILI9341
 *
 *   ����ֲ˵����
 *    1��������, ͨ������IOģ��8080ͨ��ʱ����ILI9341����ͨ�š�
 *    2�����ֵ���ʾ��ʹ�ÿ������ϵ��ⲿFLASH���ֿ�
************************************************************************************************************************************/
#include "bsp_LCD_ILI9341.h"
#include <stdio.h>
#include "stdlib.h"
#include "font.h"

typedef struct // LCD��Ҫ������
{
    uint8_t FlagInit; // ��ʼ����ɱ�־
    uint16_t width;   // LCD ����
    uint16_t height;  // LCD �߶�
    uint8_t dir;      // ���������������ƣ�0��������1������
    uint16_t id;      // LCD ID
} xLCD_TypeDef;
xLCD_TypeDef xLCD; // ����LCD��Ҫ����

/*****************************************************************************
 ** ��������
 *****************************************************************************/

// BL
#define LCD_BL_ON LCD_BL_GPIO->BSRR = LCD_BL_PIN; // �øߵ�ƽ
#define LCD_BL_OFF LCD_BL_GPIO->BRR = LCD_BL_PIN; // �õ͵�ƽ
// RD
#define LCD_RD_HIGH LCD_RD_GPIO->BSRR = LCD_RD_PIN // �øߵ�ƽ
#define LCD_RD_LOW LCD_RD_GPIO->BRR = LCD_RD_PIN   // �õ͵�ƽ
// WE
#define LCD_WE_HIGH LCD_WE_GPIO->BSRR = LCD_WE_PIN // �øߵ�ƽ
#define LCD_WE_LOW LCD_WE_GPIO->BRR = LCD_WE_PIN   // �õ͵�ƽ
// RS
#define LCD_RS_HIGH LCD_RS_GPIO->BSRR = LCD_RS_PIN // �øߵ�ƽ
#define LCD_RS_LOW LCD_RS_GPIO->BRR = LCD_RS_PIN   // �õ͵�ƽ
// CS
#define LCD_CS_HIGH LCD_CS_GPIO->BSRR = LCD_CS_PIN // �øߵ�ƽ
#define LCD_CS_LOW LCD_CS_GPIO->BRR = LCD_CS_PIN   // �õ͵�ƽ

/*****************************************************************************
 ** ��������
 ****************************************************************************/
void sendOrder(uint16_t data);                                                  // ��LCD���ͣ��Ĵ�����ֵַ(ָ��), 8λ
void sendDataShort(uint16_t data);                                              // ��LCD���ͣ����ݣ�8λ
uint16_t readData(void);                                                        // ��ȡLCD���ص�����
void sendShort(uint16_t Data);                                                  // ��Һ�������ͣ����ݣ�16λ
void setCursor(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd); // ������ʾ����
uint16_t readReg(uint16_t reg);

// ����US������ʱ������������ֲʱ���ⲿ�ļ�������
#if 0
static void delayUS(uint32_t times)
{
    times = times * 7;  //  10us����7;
    while (--times)
        __nop();
}
#endif

/******************************************************************************
 * ��  ���� delay_ms
 * ��  �ܣ� ms ��ʱ����
 * ��  ע�� 1��ϵͳʱ��72MHz
 *          2���򹴣�Options/ c++ / One ELF Section per Function
            3�������Ż�����Level 3(-O3)
 * ��  ���� uint32_t  ms  ����ֵ
 * ����ֵ�� ��
 ******************************************************************************/
static volatile uint32_t ulTimesMS; // ʹ��volatile��������ֹ�������������Ż�
static void delay_ms(uint16_t ms)
{
    ulTimesMS = ms * 1600;
    while (ulTimesMS)
        ulTimesMS--;
}

// �ײ㺯��-1����LCD���ͼĴ�����ַ(ָ��)
void sendOrder(uint16_t data)
{
    LCD_RS_LOW;        // RS=0:ָ�RS=1:����
    LCD_CS_LOW;        // Ƭѡ�ź����ͣ�ѡ���豸
    GPIOB->ODR = data; // ��ֵ����
    LCD_WE_LOW;        // WR=0���豸���Զ�ȡ���ݣ�WR=1���豸�ȴ���ȡ����
    LCD_WE_HIGH;
    LCD_CS_HIGH; // Ƭѡ�ź����ߣ�
}

// �ײ㺯��-2
// д���ݺ���
// �������sendDataShortX��,��ʱ�任�ռ�.
// data:�Ĵ���ֵ
void sendDataShort(uint16_t data)
{
    LCD_RS_HIGH;       // RS��: ����ֵ�� RS��: �Ĵ�����ֵַ
    LCD_CS_LOW;        // Ƭѡ
    GPIOB->ODR = data; // PB0~15��Ϊ������
    LCD_WE_LOW;        // д����
    LCD_WE_HIGH;       // д����
    LCD_CS_HIGH;       // Ƭѡ
}

// �ײ㺯��-3
// ��LCD����
// ����ֵ:������ֵ
uint16_t readData(void)
{
    // PB0~15������������
    GPIOB->CRL = 0X88888888; // PB0-7  ��������
    GPIOB->CRH = 0X88888888; // PB8-15 ��������
    GPIOB->BRR = 0XFFFF;     // ȫ�����0
    // ��ȡ����
    LCD_RS_HIGH;
    LCD_CS_LOW;
    LCD_RD_LOW;
    // delay_us(2)
    uint16_t reg = GPIOB->IDR;
    LCD_RD_HIGH;
    LCD_CS_HIGH;
    // ��PB0~15����Ϊ���������
    GPIOB->CRL = 0X11111111;
    GPIOB->CRH = 0X11111111; // PB8-15 �������
    GPIOB->BSRR = 0XFFFF;    // ȫ�������
    // ����
    return reg;
}

// ���Ĵ���
// LCD_Reg:�Ĵ������
// ����ֵ:������ֵ
uint16_t readReg(uint16_t reg)
{
    sendOrder(reg); // �Ĵ���
    return readData();
}

// ��ILI93xx����������ΪGBR��ʽ��������д���ʱ��ΪRGB��ʽ��
// ͨ���ú���ת��
// c:GBR��ʽ����ɫֵ
// ����ֵ��RGB��ʽ����ɫֵ
uint16_t LCD_BGR2RGB(uint16_t c)
{
    uint16_t r, g, b, rgb;
    b = (c >> 0) & 0x1f;
    g = (c >> 5) & 0x3f;
    r = (c >> 11) & 0x1f;
    rgb = (b << 11) + (g << 5) + (r << 0);
    return (rgb);
}

// ��mdk -O1ʱ���Ż�ʱ��Ҫ����
// ��ʱi
void opt_delay(uint8_t i)
{
    while (i--)
        ;
}

// ��ȡ��ĳ�����ɫֵ
// x,y:����
// ����ֵ:�˵����ɫ
uint16_t LCD_ReadPoint(uint16_t x, uint16_t y)
{
    uint16_t r, g, b;

    if (x >= xLCD.width || y >= xLCD.height)
        return 0; // �����˷�Χ,ֱ�ӷ���
    setCursor(x, y, x + 1, y + 1);
    sendOrder(0X2E);         // ��GRAMָ��
    GPIOB->CRL = 0X88888888; // PB0-7  ��������
    GPIOB->CRH = 0X88888888; // PB8-15 ��������
    GPIOB->BSRR = 0XFFFF;    // ȫ�������

    LCD_RS_HIGH;
    LCD_CS_LOW;
    // ��ȡ����(��GRAMʱ,��һ��Ϊ�ٶ�)
    LCD_RD_LOW;
    opt_delay(2);   // ��ʱ
    r = GPIOB->IDR; // ʵ��������ɫ
    LCD_RD_HIGH;

    // dummy READ
    LCD_RD_LOW;
    opt_delay(2);   // ��ʱ
    r = GPIOB->IDR; // ʵ��������ɫ
    LCD_RD_HIGH;

    LCD_RD_LOW;
    opt_delay(2);   // ��ʱ
    b = GPIOB->IDR; // ��ȡ��ɫֵ
    LCD_RD_HIGH;
    g = r & 0XFF; // ����9341,��һ�ζ�ȡ����RG��ֵ,R��ǰ,G�ں�,��ռ8λ
    g <<= 8;

    LCD_CS_HIGH;
    GPIOB->CRL = 0X11111111;
    GPIOB->CRH = 0X11111111; // PB8-15 �������
    GPIOB->BSRR = 0XFFFF;    // ȫ�������

    return (((r >> 11) << 11) | ((g >> 10) << 5) | (b >> 11)); // ��ʽת��
}

/*****************************************************************
 * ��  ����setCursor
 * ��  �ܣ�������ʾ�����ڴ�����д�������Զ�����
 * ��  ����xStart���������, yStart���������
 *         xEnd�����������㣬yEnd������������
 * ����ֵ����
 *
 ******************************************************************/
void setCursor(uint16_t xStart, uint16_t yStart, uint16_t xEnd, uint16_t yEnd)
{
#if 0           // ʹ������д��
    sendOrder(0X2A);                  // ����ָ�����x����
    sendDataShort(xStart >> 8);
    sendDataShort(xStart & 0xFF);
    sendDataShort(xEnd >> 8);
    sendDataShort(xEnd & 0xFF);

    sendOrder(0X2B);
    sendDataShort(yStart >> 8);
    sendDataShort(yStart & 0xFF);
    sendDataShort(yEnd >> 8);
    sendDataShort(yEnd & 0xFF);

    // ����дGRAMָ��
    sendOrder(0X2C);
#else           // �������ָ���⡢��
    LCD_CS_LOW; // Ƭѡ�ź����ͣ�ѡ���豸

    LCD_RS_LOW;        // RS=0:ָ�RS=1:����
    GPIOB->ODR = 0X2A; // ��ֵ����
    LCD_WE_LOW;        // WR=0���豸���Զ�ȡ���ݣ�WR=1���豸�ȴ���ȡ����
    LCD_WE_HIGH;

    LCD_RS_HIGH;              // RS��: ����ֵ�� RS��: �Ĵ�����ֵַ
    GPIOB->ODR = xStart >> 8; // PB0~15��Ϊ������
    LCD_WE_LOW;               // д����
    LCD_WE_HIGH;              // д����

    GPIOB->ODR = xStart & 0xFF; // PB0~15��Ϊ������
    LCD_WE_LOW;                 // д����
    LCD_WE_HIGH;                // д����

    GPIOB->ODR = xEnd >> 8; // PB0~15��Ϊ������
    LCD_WE_LOW;             // д����
    LCD_WE_HIGH;            // д����

    GPIOB->ODR = xEnd & 0xFF; // PB0~15��Ϊ������
    LCD_WE_LOW;               // д����
    LCD_WE_HIGH;              // д����

    LCD_RS_LOW;        // RS=0:ָ�RS=1:����
    GPIOB->ODR = 0X2B; // ��ֵ����
    LCD_WE_LOW;        // WR=0���豸���Զ�ȡ���ݣ�WR=1���豸�ȴ���ȡ����
    LCD_WE_HIGH;

    LCD_RS_HIGH;              // RS��: ����ֵ�� RS��: �Ĵ�����ֵַ
    GPIOB->ODR = yStart >> 8; // PB0~15��Ϊ������
    LCD_WE_LOW;               // д����
    LCD_WE_HIGH;              // д����

    GPIOB->ODR = yStart & 0xFF; // PB0~15��Ϊ������
    LCD_WE_LOW;                 // д����
    LCD_WE_HIGH;                // д����

    GPIOB->ODR = yEnd >> 8; // PB0~15��Ϊ������
    LCD_WE_LOW;             // д����
    LCD_WE_HIGH;            // д����

    GPIOB->ODR = yEnd & 0xFF; // PB0~15��Ϊ������
    LCD_WE_LOW;               // д����
    LCD_WE_HIGH;              // д����

    // ����дGRAMָ��
    LCD_RS_LOW;        // RS=0:ָ�RS=1:����
    GPIOB->ODR = 0X2C; // ��ֵ����
    LCD_WE_LOW;        // WR=0���豸���Զ�ȡ���ݣ�WR=1���豸�ȴ���ȡ����
    LCD_WE_HIGH;

    LCD_CS_HIGH; // Ƭѡ�ź����ߣ�
#endif
}

/*****************************************************************************
 *��  ����LCD_Init
 *��  �ܣ���ʼ��lcd
 *��  ����Ϊ�˼����⡢������ֲ������(��Ļ���ء���ʾ����)��h�ļ�������޸�
 *����ֵ����
 *��  ע��
 *****************************************************************************/
void LCD_Init(void)
{
    // �޸ĵ��Է�ʽ�����ͷ�PB3,PB4
    // ʹ��CubeMX����debguΪSerial Wireʱ��������������
    // RCC->APB2ENR |= 1 << 0;   // ��������ʱ��
    // AFIO->MAPR &= 0XF8FFFFFF; // ��0MAPR��[26:24]
    // AFIO->MAPR |= 0x2 << 24;  // ����ģʽ  000:ȫ��   010��ֻ��SWD   100:ȫ��

    // ʹ�ܸ��˿�ʱ��
    // BL����
    if (LCD_BL_GPIO == GPIOA)
        RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; // ʹ��GPIO��GPIOA
    if (LCD_BL_GPIO == GPIOB)
        RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; // ʹ��GPIO��GPIOB
    if (LCD_BL_GPIO == GPIOC)
        RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; // ʹ��GPIO��GPIOC
    if (LCD_BL_GPIO == GPIOD)
        RCC->APB2ENR |= RCC_APB2ENR_IOPDEN; // ʹ��GPIO��GPIOD
    if (LCD_BL_GPIO == GPIOE)
        RCC->APB2ENR |= RCC_APB2ENR_IOPEEN; // ʹ��GPIO��GPIOE
    if (LCD_BL_GPIO == GPIOF)
        RCC->APB2ENR |= RCC_APB2ENR_IOPFEN; // ʹ��GPIO��GPIOF
    if (LCD_BL_GPIO == GPIOG)
        RCC->APB2ENR |= RCC_APB2ENR_IOPGEN; // ʹ��GPIO��GPIOG
    // RD����
    if (LCD_BL_GPIO == GPIOA)
        RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; // ʹ��GPIO��GPIOA
    if (LCD_BL_GPIO == GPIOB)
        RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; // ʹ��GPIO��GPIOB
    if (LCD_BL_GPIO == GPIOC)
        RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; // ʹ��GPIO��GPIOC
    if (LCD_BL_GPIO == GPIOD)
        RCC->APB2ENR |= RCC_APB2ENR_IOPDEN; // ʹ��GPIO��GPIOD
    if (LCD_BL_GPIO == GPIOE)
        RCC->APB2ENR |= RCC_APB2ENR_IOPEEN; // ʹ��GPIO��GPIOE
    if (LCD_BL_GPIO == GPIOF)
        RCC->APB2ENR |= RCC_APB2ENR_IOPFEN; // ʹ��GPIO��GPIOF
    if (LCD_BL_GPIO == GPIOG)
        RCC->APB2ENR |= RCC_APB2ENR_IOPGEN; // ʹ��GPIO��GPIOG
    // WE����
    if (LCD_BL_GPIO == GPIOA)
        RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; // ʹ��GPIO��GPIOA
    if (LCD_BL_GPIO == GPIOB)
        RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; // ʹ��GPIO��GPIOB
    if (LCD_BL_GPIO == GPIOC)
        RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; // ʹ��GPIO��GPIOC
    if (LCD_BL_GPIO == GPIOD)
        RCC->APB2ENR |= RCC_APB2ENR_IOPDEN; // ʹ��GPIO��GPIOD
    if (LCD_BL_GPIO == GPIOE)
        RCC->APB2ENR |= RCC_APB2ENR_IOPEEN; // ʹ��GPIO��GPIOE
    if (LCD_BL_GPIO == GPIOF)
        RCC->APB2ENR |= RCC_APB2ENR_IOPFEN; // ʹ��GPIO��GPIOF
    if (LCD_BL_GPIO == GPIOG)
        RCC->APB2ENR |= RCC_APB2ENR_IOPGEN; // ʹ��GPIO��GPIOG
    // RS����
    if (LCD_BL_GPIO == GPIOA)
        RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; // ʹ��GPIO��GPIOA
    if (LCD_BL_GPIO == GPIOB)
        RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; // ʹ��GPIO��GPIOB
    if (LCD_BL_GPIO == GPIOC)
        RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; // ʹ��GPIO��GPIOC
    if (LCD_BL_GPIO == GPIOD)
        RCC->APB2ENR |= RCC_APB2ENR_IOPDEN; // ʹ��GPIO��GPIOD
    if (LCD_BL_GPIO == GPIOE)
        RCC->APB2ENR |= RCC_APB2ENR_IOPEEN; // ʹ��GPIO��GPIOE
    if (LCD_BL_GPIO == GPIOF)
        RCC->APB2ENR |= RCC_APB2ENR_IOPFEN; // ʹ��GPIO��GPIOF
    if (LCD_BL_GPIO == GPIOG)
        RCC->APB2ENR |= RCC_APB2ENR_IOPGEN; // ʹ��GPIO��GPIOG
    // CS����
    if (LCD_BL_GPIO == GPIOA)
        RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; // ʹ��GPIO��GPIOA
    if (LCD_BL_GPIO == GPIOB)
        RCC->APB2ENR |= RCC_APB2ENR_IOPBEN; // ʹ��GPIO��GPIOB
    if (LCD_BL_GPIO == GPIOC)
        RCC->APB2ENR |= RCC_APB2ENR_IOPCEN; // ʹ��GPIO��GPIOC
    if (LCD_BL_GPIO == GPIOD)
        RCC->APB2ENR |= RCC_APB2ENR_IOPDEN; // ʹ��GPIO��GPIOD
    if (LCD_BL_GPIO == GPIOE)
        RCC->APB2ENR |= RCC_APB2ENR_IOPEEN; // ʹ��GPIO��GPIOE
    if (LCD_BL_GPIO == GPIOF)
        RCC->APB2ENR |= RCC_APB2ENR_IOPFEN; // ʹ��GPIO��GPIOF
    if (LCD_BL_GPIO == GPIOG)
        RCC->APB2ENR |= RCC_APB2ENR_IOPGEN; // ʹ��GPIO��GPIOG

    // GPIO_InitTypeDef    GPIO_InitStruct = {0};       // ������ʼ��Ҫ�õ��Ľṹ��

    // GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;     // ����ģʽ
    // GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;    // ��������

    // // �����������ã�BL_PA15
    // GPIO_InitStruct.Pin = LCD_BL_PIN;                // ���ű��
    // HAL_GPIO_Init(LCD_BL_GPIO, &GPIO_InitStruct);    // ��ʼ��
    // // �����������ų�ʼ���� RD_PC6
    // GPIO_InitStruct.Pin  = LCD_RD_PIN ;              // ���ű��
    // HAL_GPIO_Init(LCD_RD_GPIO, &GPIO_InitStruct);    // ��ʼ��
    // // �����������ų�ʼ���� WE_PC7
    // GPIO_InitStruct.Pin  =  LCD_WE_PIN;              // ���ű��
    // HAL_GPIO_Init(LCD_WE_GPIO, &GPIO_InitStruct);    // ��ʼ��
    // // �����������ų�ʼ���� RS_PC8
    // GPIO_InitStruct.Pin  = LCD_RS_PIN;               // ���ű��
    // HAL_GPIO_Init(LCD_RS_GPIO, &GPIO_InitStruct);    // ��ʼ��
    // // �����������ų�ʼ���� CS_PC9
    // GPIO_InitStruct.Pin  =  LCD_CS_PIN;              // ���ű��
    // HAL_GPIO_Init(LCD_CS_GPIO, &GPIO_InitStruct);    // ��ʼ��



    GPIOA->CRH |= GPIO_CRH_MODE15;
    GPIOA->CRH &= ~GPIO_CRH_CNF15;

    // 3. ��ʼ��GPIOC��4�����ţ�PC6-PC9��
    // uint16_t lcdPins = LCD_RD_PIN | LCD_WE_PIN | LCD_RS_PIN | LCD_CS_PIN;
    // GPIO_InitStruct.GPIO_Pin = lcdPins;
    // GPIO_Init(LCD_RD_GPIO, &GPIO_InitStruct); // ��������ͬ��GPIOC��һ���Գ�ʼ��

    GPIOC->CRL |= GPIO_CRL_MODE6;
    GPIOC->CRL &= ~GPIO_CRL_CNF6;

    GPIOC->CRL |= GPIO_CRL_MODE7;
    GPIOC->CRL &= ~GPIO_CRL_CNF7;

    GPIOC->CRH |= GPIO_CRH_MODE8;
    GPIOC->CRH &= ~GPIO_CRH_CNF8;

    GPIOC->CRH |= GPIO_CRH_MODE9;
    GPIOC->CRH &= ~GPIO_CRH_CNF9;

    // ���ݴ������ų�ʼ���� PB0~PB15
    GPIOB->CRL = 0X11111111;
    GPIOB->CRH = 0X11111111; // PB8-15  �������
    GPIOB->BSRR = GPIO_PIN_All;

    delay_ms(50); // delay 50 ms

    // ����9341 ID�Ķ�ȡ
    sendOrder(0XD3);      // ָ���ID
    readData();           // ��1��������dummy
    readData();           // ��2��������IC�汾��
    xLCD.id = readData(); // ��3��������IC����(93)
    xLCD.id <<= 8;
    xLCD.id |= readData(); // ��4��������IC����(41)

    //printf("��ʾ�� ���...        %x\r\n", xLCD.id); // ��ӡLCD ID
    if (xLCD.id != 0X9341)                           // 9341��ʼ��ʧ��
        return;

    sendOrder(0xCF);
    sendDataShort(0x00);
    sendDataShort(0xC1);
    sendDataShort(0X30);
    sendOrder(0xED);
    sendDataShort(0x64);
    sendDataShort(0x03);
    sendDataShort(0X12);
    sendDataShort(0X81);
    sendOrder(0xE8);
    sendDataShort(0x85);
    sendDataShort(0x10);
    sendDataShort(0x7A);
    sendOrder(0xCB);
    sendDataShort(0x39);
    sendDataShort(0x2C);
    sendDataShort(0x00);
    sendDataShort(0x34);
    sendDataShort(0x02);
    sendOrder(0xF7);
    sendDataShort(0x20);
    sendOrder(0xEA);
    sendDataShort(0x00);
    sendDataShort(0x00);
    sendOrder(0xC0);     // Power control
    sendDataShort(0x1B); // VRH[5:0]
    sendOrder(0xC1);     // Power control
    sendDataShort(0x01); // SAP[2:0];BT[3:0]
    sendOrder(0xC5);     // VCM control
    sendDataShort(0x30); // 3F
    sendDataShort(0x30); // 3C
    sendOrder(0xC7);     // VCM control2
    sendDataShort(0XB7);
    sendOrder(0x36); // Memory Access Control
    sendDataShort(0x48);
    sendOrder(0x3A);
    sendDataShort(0x55);
    sendOrder(0xB1);
    sendDataShort(0x00);
    sendDataShort(0x1A);
    sendOrder(0xB6); // Display Function Control
    sendDataShort(0x0A);
    sendDataShort(0xA2);
    sendOrder(0xF2); // 3Gamma Function Disable
    sendDataShort(0x00);
    sendOrder(0x26); // Gamma curve selected
    sendDataShort(0x01);
    sendOrder(0xE0); // Set Gamma
    sendDataShort(0x0F);
    sendDataShort(0x2A);
    sendDataShort(0x28);
    sendDataShort(0x08);
    sendDataShort(0x0E);
    sendDataShort(0x08);
    sendDataShort(0x54);
    sendDataShort(0XA9);
    sendDataShort(0x43);
    sendDataShort(0x0A);
    sendDataShort(0x0F);
    sendDataShort(0x00);
    sendDataShort(0x00);
    sendDataShort(0x00);
    sendDataShort(0x00);
    sendOrder(0XE1); // Set Gamma
    sendDataShort(0x00);
    sendDataShort(0x15);
    sendDataShort(0x17);
    sendDataShort(0x07);
    sendDataShort(0x11);
    sendDataShort(0x06);
    sendDataShort(0x2B);
    sendDataShort(0x56);
    sendDataShort(0x3C);
    sendDataShort(0x05);
    sendDataShort(0x10);
    sendDataShort(0x0F);
    sendDataShort(0x3F);
    sendDataShort(0x3F);
    sendDataShort(0x0F);
    sendOrder(0x2B);
    sendDataShort(0x00);
    sendDataShort(0x00);
    sendDataShort(0x01);
    sendDataShort(0x3f);
    sendOrder(0x2A);
    sendDataShort(0x00);
    sendDataShort(0x00);
    sendDataShort(0x00);
    sendDataShort(0xef);
    sendOrder(0x11); // Exit Sleep
    delay_ms(50);
    sendOrder(0x29); // display on

    LCD_SetDir(0);
    LCD_Fill(0, 0, xLCD.width - 1, xLCD.height - 1, BLACK);
    LCD_BL_ON; // �򿪱���LED
    xLCD.FlagInit = 1;
}

/******************************************************************
 * �������� LCD_SetDir
 * ��  �ܣ� ������ʾ����
 * ��  ���� uint8_t dir     0-������1-����
 * ��  ע�� ���ʹ�ô�������ÿ�θ�������󣬶���Ҫ����У׼
 *          �����ļĴ�������ֵ�� 0-��������3-��������5-������, 6-������; ע�⣺���ʹ�ô�������ÿ�θ�������󣬶���Ҫ����У׼
 * ��  �أ� ��
 *****************************************************************/
void LCD_SetDir(uint8_t dir)
{
    uint16_t regval = 0;

    if (dir == 1)
        dir = 6;

    if (dir == 0 || dir == 3) // ����
    {
        xLCD.dir = 0;
        xLCD.width = LCD_WIDTH;
        xLCD.height = LCD_HIGH;
    }
    if (dir == 5 || dir == 6) // ����
    {
        xLCD.dir = 1;
        xLCD.width = LCD_HIGH;
        xLCD.height = LCD_WIDTH;
    }

    if (dir == 0)
        regval |= (0 << 7) | (0 << 6) | (0 << 5); // ������,���ϵ���
    if (dir == 3)
        regval |= (1 << 7) | (1 << 6) | (0 << 5); // ���ҵ���,���µ���
    if (dir == 5)
        regval |= (0 << 7) | (1 << 6) | (1 << 5); // ���ϵ���,���ҵ���
    if (dir == 6)
        regval |= (1 << 7) | (0 << 6) | (1 << 5); // ���µ���,������
    sendOrder(0X36);                              // ��д������ɫģʽ
    sendDataShort(regval | 0x08);                 //
}

// LCD������ʾ
void LCD_DisplayOn(void)
{
    sendOrder(0X29); // ������ʾ
    LCD_BL_ON;       // ��������LED
}

// LCD�ر���ʾ
void LCD_DisplayOff(void)
{
    sendOrder(0X28); // �ر���ʾ
    LCD_BL_OFF;      // �رձ���LED
}

/******************************************************************
 * �������� LCD_GetDir
 * ��  �ܣ� ��ȡ������ʾ����
 * ��  ���� ��
 * �����أ� uint8_t  dir   ; 0-������1-����
 * ��  ע��
 *****************************************************************/
uint8_t LCD_GetDir(void)
{
    return xLCD.dir;
}

/******************************************************************
 * �������� LCD_GetWidth
 * ��  �ܣ� ��ȡ���Ŀ���; ��λ:����
 * ��  ���� ��
 * �����أ� uint16_t  width  �����Ŀ���; ��λ������
 * ��  ע��
 *****************************************************************/
uint16_t LCD_GetWidth(void)
{
    return xLCD.width;
}

/******************************************************************
 * �������� LCD_GetHeight
 * ��  �ܣ� ��ȡ���ĸ߶�; ��λ:����
 * ��  ���� ��
 * �����أ� uint16_t  height  �����ĸ߶�;
 * ��  ע��
 *****************************************************************/
uint16_t LCD_GetHeight(void)
{
    return xLCD.height;
}

/*****************************************************************
 * ��  ����drawPoint
 * ��  �ܣ���һ����
 * ��  ����x���꣬y����, 16λ��ɫֵ
 * ����ֵ����
 *
 ******************************************************************/
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
#if 1           /* 使用setCursor，正确发送4字节窗口参数 */
    setCursor(x, y, x + 1, y + 1);   //���ù��λ��
    sendDataShort(color);
#else           // ������д��������
    LCD_CS_LOW; // Ƭѡ�ź����ͣ�ѡ���豸

    LCD_RS_LOW;        // RS=0:ָ�RS=1:����
    GPIOB->ODR = 0X2A; // ��ֵ����
    LCD_WE_LOW;        // WR=0���豸���Զ�ȡ���ݣ�WR=1���豸�ȴ���ȡ����
    LCD_WE_HIGH;

    LCD_RS_HIGH;         // RS��: ����ֵ�� RS��: �Ĵ�����ֵַ
    GPIOB->ODR = x >> 8; // PB0~15��Ϊ������
    LCD_WE_LOW;          // д����
    LCD_WE_HIGH;         // д����

    GPIOB->ODR = x & 0xFF; // PB0~15��Ϊ������
    LCD_WE_LOW;            // д����
    LCD_WE_HIGH;           // д����

    LCD_RS_LOW;        // RS=0:ָ�RS=1:����
    GPIOB->ODR = 0X2B; // ��ֵ����
    LCD_WE_LOW;        // WR=0���豸���Զ�ȡ���ݣ�WR=1���豸�ȴ���ȡ����
    LCD_WE_HIGH;

    LCD_RS_HIGH;         // RS��: ����ֵ�� RS��: �Ĵ�����ֵַ
    GPIOB->ODR = y >> 8; // PB0~15��Ϊ������
    LCD_WE_LOW;          // д����
    LCD_WE_HIGH;         // д����

    GPIOB->ODR = y & 0xFF; // PB0~15��Ϊ������
    LCD_WE_LOW;            // д����
    LCD_WE_HIGH;           // д����

    // ����дGRAMָ��
    LCD_RS_LOW;        // RS=0:ָ�RS=1:����
    GPIOB->ODR = 0X2C; // ��ֵ����
    LCD_WE_LOW;        // WR=0���豸���Զ�ȡ���ݣ�WR=1���豸�ȴ���ȡ����
    LCD_WE_HIGH;
    // ��ɫֵ
    LCD_RS_HIGH;        // RS��: ����ֵ�� RS��: �Ĵ�����ֵַ
    GPIOB->ODR = color; // PB0~15��Ϊ������
    LCD_WE_LOW;         // д����
    LCD_WE_HIGH;        // д����

    LCD_CS_HIGH; // Ƭѡ
#endif
}

// ��ָ��λ�û�һ��ָ����С��Բ
//(x,y):���ĵ�
// r    :�뾶
void LCD_Circle(uint16_t x0, uint16_t y0, uint8_t r, uint16_t color)
{
    int a, b;
    int di;
    a = 0;
    b = r;
    di = 3 - (r << 1); // �ж��¸���λ�õı�־
    while (a <= b)
    {
        LCD_DrawPoint(x0 + a, y0 - b, color); // 5
        LCD_DrawPoint(x0 + b, y0 - a, color); // 0
        LCD_DrawPoint(x0 + b, y0 + a, color); // 4
        LCD_DrawPoint(x0 + a, y0 + b, color); // 6
        LCD_DrawPoint(x0 - a, y0 + b, color); // 1
        LCD_DrawPoint(x0 - b, y0 + a, color);
        LCD_DrawPoint(x0 - a, y0 - b, color); // 2
        LCD_DrawPoint(x0 - b, y0 - a, color); // 7
        a++;
        // ʹ��Bresenham�㷨��Բ
        if (di < 0)
            di += 4 * a + 6;
        else
        {
            di += 10 + 4 * (a - b);
            b--;
        }
    }
}

// ����
// x1,y1:�������
// x2,y2:�յ�����
void LCD_Line(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;
    delta_x = x2 - x1; // ������������
    delta_y = y2 - y1;
    uRow = x1;
    uCol = y1;
    if (delta_x > 0)
        incx = 1; // ���õ�������
    else if (delta_x == 0)
        incx = 0; // ��ֱ��
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }
    if (delta_y > 0)
        incy = 1;
    else if (delta_y == 0)
        incy = 0; // ˮƽ��
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }
    if (delta_x > delta_y)
        distance = delta_x; // ѡȡ��������������
    else
        distance = delta_y;
    for (t = 0; t <= distance + 1; t++) // �������
    {
        LCD_DrawPoint(uRow, uCol, color); // ����
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
}

/*****************************************************************
 * ��  ����LCD_Rectangle
 * ��  �ܣ�������
 * ��  ����
 * ����ֵ����
 ******************************************************************/
void LCD_Rectangle(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    LCD_Line(x1, y1, x2, y1, color);
    LCD_Line(x1, y1, x1, y2, color);
    LCD_Line(x1, y2, x2, y2, color);
    LCD_Line(x2, y1, x2, y2, color);
}

/*****************************************************************
 * ��  ����LCD_Fill
 * ��  �ܣ���ָ�����������ָ����ɫ
 * ��  ����
 * ����ֵ����
 ******************************************************************/
void LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t color)
{
    uint32_t CNT = (ex + 1 - sx) * (ey + 1 - sy);
    setCursor(sx, sy, ex, ey); // ���ù��λ��

    LCD_RS_HIGH; // RS��: ����ֵ�� RS��: �Ĵ�����ֵַ
    LCD_CS_LOW;  // Ƭѡ
    while (CNT--)
    {
        GPIOB->ODR = color; // PB0~15��Ϊ������
        LCD_WE_LOW;         // д����
        LCD_WE_HIGH;        // д����
    }
    LCD_CS_HIGH; // Ƭѡ
}

/******************************************************************
 * �������� LCD_Cross
 * ��  �ܣ� ��ָ�����ϻ���ʮ���ߣ�����У׼������
 * ��  ���� uint16_t x  ��   ʮ���ߵ����ĵ�����x
 *          uint16_t y  ��   ʮ���ߵ����ĵ�����x
 *          uint16_t len     ʮ���ߵ����س���
 *          uint32_t fColor  ��ɫ
 * �����أ� ��
 * ��  ע��
 *****************************************************************/
void LCD_Cross(uint16_t x, uint16_t y, uint16_t len, uint32_t fColor)
{
    uint16_t temp = len / 2;

    LCD_Line(x - temp, y, x + temp, y, fColor);
    LCD_Line(x, y - temp, x, y + temp, fColor);
}

/******************************************************************
 * �������� drawAscii
 * ��  �ܣ� ��ָ��λ����ʾһ���ַ�
 * ��  ���� uint16_t x,y     ��ʼ����
 *          uint8_t  num     Ҫ��ʾ���ַ�:" "--->"~"
 *          uint8_t  size    �����С 12/16/24/32
 *          uint32_t fColor  ������ɫ
 *          uint32_t bColor  ������ɫ
 * ��  ע�� �ο�ԭ�Ӹ��Ұ�����Ĵ�����޸�
 *****************************************************************/
void drawAscii(uint16_t x, uint16_t y, uint8_t num, uint8_t size, uint32_t fColor, uint32_t bColor)
{
    // spiInit();                                              // ��ֹSPI�����������豸�޸���
    static uint8_t temp;
    static uint8_t csize;
    static uint16_t y0;

    y0 = y;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2); // �õ�����һ���ַ���Ӧ������ռ���ֽ���
    num = num - ' ';                                        // �õ�ƫ�ƺ��ֵ��ASCII�ֿ��Ǵӿո�ʼȡģ������-' '���Ƕ�Ӧ�ַ����ֿ⣩
    for (uint8_t t = 0; t < csize; t++)
    {
        if (size == 12)
            temp = aFontASCII12[num][t]; // ����1206����
        else if (size == 16)
            temp = aFontASCII16[num][t]; // ����1608����
        else if (size == 24)
            temp = aFontASCII24[num][t]; // ����2412����
        else if (size == 32)
#if FONT_ASSIC_32_EN
            temp = aFontASCII32[num][t];
#else
        {
            if (t == 0) { csize = 36; size = 24; }
            temp = aFontASCII24[num][t];
        }
#endif
        else
            return;
        for (uint8_t t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)
                LCD_DrawPoint(x, y, fColor); // ���� ����
            else
                LCD_DrawPoint(x, y, bColor); // ���� ����

            temp <<= 1;
            y++;
            if (y >= xLCD.height)
                return; // ������Ļ�߶�(��)
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                if (x >= xLCD.width)
                    return; // ������Ļ����(��)
                break;
            }
        }
    }
}

/******************************************************************
 * �������� drawGBK
 * ��  �ܣ� ��ָ��λ����ʾһ���ַ�
 * ��  ���� uint16_t x,y     ��ʼ����
 *          uint8_t  num     Ҫ��ʾ���ַ�:" "--->"~"
 *          uint8_t  size    �����С 12/16/24/32
 *          uint32_t fColor  ������ɫ
 *          uint32_t bColor  ������ɫ
 * ��  ע�� �ο�ԭ�Ӹ��Ұ�����Ĵ�����޸�
 *****************************************************************/
#if 0
void drawGBK(uint16_t x, uint16_t y, uint8_t *font, uint8_t size, uint32_t fColor, uint32_t bColor)
{
    static uint8_t temp;
    static uint16_t y0;
    static uint8_t GBK[128];
    static uint8_t csize;

    csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size);      // �õ�����һ���ַ���Ӧ������ռ���ֽ���
    W25Q128_ReadFontData(font, size, GBK);                   // �õ���Ӧ��С�ĵ�������
                                                             
    //spiInit();                                             // ��ֹSPI�����������豸�޸���
    y0 = y;                                                  
    for (uint8_t t = 0; t < csize; t++)                      
    {                                                        
        temp = GBK[t];                                       // �õ�GBK��������
        for (uint8_t t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)   LCD_DrawPoint(x, y, fColor);
            else            LCD_DrawPoint(x, y, bColor);
            temp <<= 1;
            y++;
            if ((y - y0) == size)
            {
                y = y0;
                x++;
                break;
            }
        }
    }
}
#endif

/******************************************************************************
 * ��  ���� LCD_String
 * ��  �ܣ� ��LCD����ʾ�ַ���(֧��Ӣ�ġ�����)
 * ��  ���� Ӣ�ģ���ģ���ݱ�����font.h�������ʹ���һ�𱣴���оƬ�ڲ�Flash
 *          ���֣���ģ�������ⲿFlash�У��������ֿ���W25Q128��
 *                ħŮ��������W25Q128����¼����4���ֺŴ�С��ģ����
 * ��  ���� uint16_t   x      �������Ͻ�X����
 *          uint16_t   y      �������Ͻ�y����
 *          char* pFont  Ҫ��ʾ���ַ�������
 *          uint8_t    size   �ֺŴ�С��12 16 24 32
 *          uint32_t   fColor ������ɫ
 *          uint32_t   bColor ������ɫ
 * ����ֵ:  ��
 * ��  ע�� ����޸�_2020��05��1����
 ******************************************************************************/
void LCD_String(uint16_t x, uint16_t y, char *pFont, uint8_t size, uint32_t fColor, uint32_t bColor)
{
    
    
    if (xLCD.FlagInit == 0)
        return;

    uint16_t xStart = x;
    int8_t num;

    if (size != 12 && size != 16 && size != 24 && size != 32) // �����С����
        size = 24;

    while (*pFont != 0) // ������ȡ�ַ������ݣ�ֱ��'\0'ʱֹͣ
    {
        if (x > (xLCD.width - size)) // ��λ���жϣ����������ĩ���Ͱѹ�껻��
        {
            x = xStart;
            y = y + size;
        }
        if (y > (xLCD.height - size)) // ��λ���жϣ����������ĩ���ͷ��أ��������
            return;

        if (*pFont < 127) // ASCII�ַ�
        {
            drawAscii(x, y, *pFont, size, fColor, bColor);
            pFont++;
            x += size / 2;
        }
        else // ������ʾ
        {
            // ��Ҫ: ����õĲ���ħŮ��������ֿ�, ��Ҫ�޸Ļ�ע��������һ��, �����Ͳ�Ӱ��ASCIIӢ���ַ������
            // drawGBK(x, y, (uint8_t *)pFont, size, fColor, bColor);
            num = String_Index(pFont);
            if(num != -1)
            {
                LCD_ShowChinese(x, y, num, size, fColor, bColor);
            }
            
            pFont = pFont + 2; // ��һ��Ҫ��ʾ���������ڴ��е�λ��
            x = x + size;      // ��һ��Ҫ��ʾ����������Ļ�ϵ�Xλ��
        }
    }
}

/******************************************************************
 * �������� LCD_Image
 * ��  �ܣ� ��ָ�����������ָ��ͼƬ����
 *          ͼƬ������font.h�ļ���.ֻ�ʺ�����ͼƬ����
 *          Image2Lcdת����ˮƽɨ�裬16λ���ɫ
 * ��  ���� uint16_t x,y     ���Ͻ���ʼ����
 *          uint16_t width   ͼƬ����
 *          uint16_t height  ͼƬ�߶�
 *          uint8_t* image   ���ݻ����ַ
 *****************************************************************/
void LCD_Image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t *image)
{
    uint16_t colorData = 0;
    uint32_t cnt = 0;

    for (uint16_t i = 0; i < height; i++) // һ��һ�е���ʾ
    {
        setCursor(x, y + i, x + width, y + height); // �������ù��λ��
        for (uint16_t j = 0; j < width; j++)        // һ���У������£�������ش���
        {
            colorData = (image[cnt * 2 + 1] << 8) | image[cnt * 2];
            sendDataShort(colorData); // д��16λ��ɫ����
            cnt++;
        }
    }
}

/******************************************************************
 * �������� LCD_DispFlush
 * ��  �ܣ� ��ָ�����������ָ������
 * ��  ע�� ��������������ͼƬ������䡢16λ����λ��ǰ(�������ͼƬ��ʾ�����෴);
 *          ��������������LVGL��ֲ�ĺ�����disp_flush()������Ч�ؿ���ˢ��
 * ��  ���� uint16_t   x        ���Ͻ���ʼX����
 *          uint16_t   y        ���Ͻ���ʼY����
 *          uint16_t   width    ���ȣ�ÿ���ж��ٸ�16λ����; ��������ΪͼƬ�Ŀ�
 *          uint16_t   height   �߶ȣ�ÿ���ж��ٸ�16λ����; ��������ΪͼƬ�ĸ�
 *          uint16_t  *pData    ���ݵ�ַ
 * ��  �أ� ��
 *****************************************************************/
void LCD_DispFlush(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint16_t *pData)
{
    for (uint16_t nowY = y; nowY <= height; nowY++) // ������ʾ
    {
        setCursor(x, nowY, width, nowY);               // �������ù��λ��
        for (uint16_t nowX = x; nowX <= width; nowX++) // һ���У��������������
        {
            sendDataShort(*pData++); // д��ÿ�����16λ��ɫ����, RGB565ֵ
        }
    }
}

/******************************************************************
 * �������� LCD_ShowChinese
 * ��  �ܣ� ��ʾ����ȡģ�ĺ���,
 *          �ֿ�������font�ļ��У�ֻ�ʺ��������̶ֹ����
 *          PCtoLCD2018ȡģ������+����ʽ+����+C51��ʽ
 * ��  ���� uint16_t  x         ����x
 *          uint16_t  y         ����y
 *          uint8_t   index     ��ģ�����������е����
 *          uint32_t  fColor    ������ɫ
 *          uint32_t  bColor    ������ɫ
 * ��  ��:  ��
 *****************************************************************/
void LCD_ShowChinese(uint8_t x, uint8_t y, uint8_t num, uint8_t size1, uint32_t fColor, uint32_t bColor)
{
    uint8_t m, temp;
    uint8_t x0 = x, y0 = y;
    uint16_t size3 = (size1 / 8 + ((size1 % 8) ? 1 : 0)) * size1; // �õ�����һ���ַ���Ӧ������ռ���ֽ���

    for (uint16_t i = 0; i < size3; i++)
    {
        if (size1 == 12)
        {
            if (i == 0) { size3 = 32; size1 = 16; }
            temp = aFontChinese16[num][i];
        }
        else if (size1 == 16)
        {
            //temp = aFontChinese16[num][i]; // ����16*16����
            temp = z_GB_16[num].Msk[i];
        }
        else if (size1 == 24)
        {
            temp = aFontChinese24[num][i]; // ����24*24����
        }
        else if (size1 == 32)
        {
            if (i == 0) { size3 = 72; size1 = 24; }
            temp = aFontChinese24[num][i];
        }
        else
        {
            if (i == 0) { size3 = 32; size1 = 16; }
            temp = aFontChinese16[num][i];
        }
        for (m = 0; m < 8; m++)
        {
            if (temp & 0x01)
                LCD_DrawPoint(x, y, fColor);
            else
                LCD_DrawPoint(x, y, bColor);
            temp >>= 1;
            y++;
        }
        x++;
        if ((x - x0) == size1)
        {
            x = x0;
            y0 = y0 + 8;
        }
        y = y0;
    }
}

int8_t String_Index(const char *chinese_str)
{

    int16_t font_index = -1;
    signed char byte1 = chinese_str[0];
    signed char byte2 = chinese_str[1];
   
    for (uint16_t j = 0; j < sizeof(z_GB_16) / sizeof(z_GB_16[0]); j++)
    {
        if ((z_GB_16[j].Index[0] == byte1) &&
            (z_GB_16[j].Index[1] == byte2))
        {
            font_index = j;
            break;
        }
    }

    if (font_index != -1)
    {
        return font_index;
    }
    else
    {
        return -1;
    }
}

/******************************************************************
 * �������� LCD_GUI
 * ��  �ܣ� ���԰����豸�����LCD��ʾ����
 * ��  ����
 * �����أ�
 * ��  ע��
 *****************************************************************/
void LCD_GUI(void)
{
    char strTemp[30];

    // ȫ������-����
    LCD_Fill(0, 0, xLCD.width, xLCD.height, BLACK);

    LCD_String(8, 0, "STM32F103RCT6������", 24, WHITE, BLACK);
    LCD_String(72, 28, "�豸�����", 16, GREY, BLACK);

    // LCD_Image (0,0, 60, 60, imageLoge);  // ͼƬ��ʾ����
    //  �߿�
    LCD_Line(0, 49, 0, 329, GREY);     // ����
    LCD_Line(119, 70, 119, 329, GREY); // ����
    LCD_Line(239, 49, 239, 329, GREY); // ����

    LCD_Fill(0, 49, 239, 70, WHITE);
    LCD_String(6, 52, "�����豸", 16, BLACK, WHITE);
    LCD_String(125, 52, "WiFi����ͨ��", 16, BLACK, WHITE);

    LCD_Fill(119, 125, 239, 145, WHITE);
    LCD_String(125, 127, "CANͨ��", 16, BLACK, WHITE);

    LCD_Fill(119, 205, 239, 225, WHITE);
    LCD_String(125, 207, "RS485ͨ��", 16, BLACK, WHITE);

    // �ײ�״̬��-����
    LCD_Fill(0, 287, 239, 329, WHITE); // �װ�
    LCD_Line(0, 303, 239, 303, BLACK);
    LCD_Line(119, 287, 119, 329, BLACK);
    LCD_Line(119, 49, 119, 70, BLACK); // ����
    // �ײ�״̬��-����
    LCD_String(6, 290, "�ڲ��¶�", 12, BLACK, WHITE);   // �ڲ��¶�
    LCD_String(6, 306, "��������", 12, BLACK, WHITE);   // ��������
    LCD_String(125, 290, "��������", 12, BLACK, WHITE); // ��������
    LCD_String(125, 306, "����ʱ��", 12, BLACK, WHITE); // ����ʱ��
    sprintf(strTemp, "��%d��", 0);
    LCD_String(68, 306, strTemp, 12, BLUE, WHITE);

    uint16_t y = 74;
    // UASRT1
    //    LCD_String(6, y, "UART1����",  12, YELLOW, BLACK);
    //    if (xUSART1.InitFlag == 1)
    //    {
    //        LCD_String(90, y, "���", 12, GREEN, BLACK);
    //    }
    //    else
    //    {
    //        LCD_String(90, y, "ʧ��", 12, RED, BLACK);
    //    }
    y = y + 15;
    // SystemClock
    LCD_String(6, y, "ϵͳʱ��", 12, YELLOW, BLACK);
    sprintf(strTemp, "%d", SystemCoreClock / 1000000);
    LCD_String(84, y, strTemp, 12, GREEN, BLACK);
    LCD_String(96, y, "MHz", 12, GREEN, BLACK);
    y = y + 15;
    // LEDָʾ��
    LCD_String(6, y, "LEDָʾ��", 12, YELLOW, BLACK);
    LCD_String(90, y, "���", 12, GREEN, BLACK);
    y = y + 15;
    // �����ж�
    LCD_String(6, y, "�����ж�", 12, YELLOW, BLACK);
    LCD_String(90, y, "���", 12, GREEN, BLACK);
    y = y + 15;
    // FLASH�洢��
    LCD_String(6, y, "FLASH�洢", 12, YELLOW, BLACK);
    // if (W25Q128_GetInitStatus())
    // {
    //     LCD_String(71, y, W25Q128_GetType(), 12, GREEN, BLACK);
    // }
    // else
    // {
    //     LCD_String(90, y, "ʧ��", 12, RED, BLACK);
    // }
    y = y + 15;
    // �����ֿ�
    LCD_String(6, y, "�����ֿ�", 12, YELLOW, BLACK);
    // if (W25Q128_GetFontStorageStatus())
    // {
    //     LCD_String(90, y, "����", 12, GREEN, BLACK);
    // }
    // else
    // {
    //     LCD_String(90, y, "ʧ��", 12, RED, BLACK);
    // }
    y = y + 15;
    // ��ʾ��
    LCD_String(6, y, "��ʾ��оƬ", 12, YELLOW, BLACK);
    sprintf(strTemp, "%X", xLCD.id);
    if (xLCD.FlagInit == 1)
    {
        LCD_String(90, y, strTemp, 12, GREEN, BLACK);
    }
    else
    {
        LCD_String(90, y, "ʧ��", 12, RED, BLACK);
    }
    y = y + 15;
}
