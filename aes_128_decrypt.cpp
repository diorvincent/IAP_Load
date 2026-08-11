/*
模块说明：用于对下载到电机的App .bin文件进行AES128算法解密
时间：20260803
作者：hejingchi

修改：dongshaoqiang
修改内容：
  添加GfMultBy03,GfMultBy09,GfMultBy0B,GfMultBy0D,GfMultBy0E函数，
  修改MixColumns函数
  解决对bin文件解密时数据块大小限制(原函数由于数据块异或逻辑，数据类型原因只能对16bytes的整数倍(最大240bytes)进行解密)
时间：20260805
*/

#include "aes_128_decrypt.h"

aes_128_decrypt::aes_128_decrypt()
{
}

/*****************************************************************************
*	函数名：	RShiftWord
*	功能描述：	对一个pWord 4字节数据进行循环右移。
*	输入参数：	pWord -- 要右移的4字节数据。
*	输出参数：	pWord -- 右移后的4字节数据。
*	返回值：      无。
*****************************************************************************/
void aes_128_decrypt::RShiftWord(unsigned char *pWord)
{
    unsigned char temp = pWord[0];
    pWord[0]  = pWord[1];
    pWord[1]  = pWord[2];
    pWord[2]  = pWord[3];
    pWord[3]  = temp;
}

/*****************************************************************************
*	函数名：	XorBytes
*	功能描述：	异或两组数据。
*	输入参数：	pData1 -- 要异或的第一组数据。
*			   pData2 -- 要异或的第二组数据。
*			   nCount -- 参与异或的数据长度。
*	输出参数：	pData1 -- 异或后的结果。
*	返回值：    无。
*****************************************************************************/
void aes_128_decrypt::XorBytes(unsigned char *pData1, const unsigned char *pData2, unsigned char nCount)
{
    unsigned char i;
    for (i = 0; i < nCount; i++) {
        pData1[i] ^= pData2[i];
    }
}

/*****************************************************************************
*	函数名：	AddKey
*	功能描述：	把 pData数据 加上（异或）pKey密钥，数据长度为16字节。
*	输入参数：	pData	  -- 数据。
*			   pKey      -- 密钥。
*	输出参数：	pStpDataate	  -- 加上子密钥后的数据。
*	返回值：	无。
*****************************************************************************/
void aes_128_decrypt::AddKey(unsigned char *pData, const unsigned char *pKey)
{
    XorBytes(pData, pKey, 4 * Nb);
}

/*****************************************************************************
*	函数名：	SubstituteBytes
*	功能描述：	通过S盒子置换数据。
*	输入参数：	pData  	-- 数据。
*			   dataCnt -- 数据长度。
*			   pBox	   -- 置换盒子，加密时使用SBox, 解密时使用InvSBox
*	输出参数：	pData	-- 置换后的状态数据。
*	返回值：	无。
*****************************************************************************/
void aes_128_decrypt::SubstituteBytes(unsigned char *pData, unsigned char dataCnt, const unsigned char *pBox)
{
    unsigned char i;
    for (i = 0; i < dataCnt; i++) {
        pData[i] = pBox[pData[i]];
    }
}

/*****************************************************************************
*	函数名：	ShiftRows
*	功能描述：	把状态数据移行。
*	输入参数：	pState	-- 状态数据。
*			   bInvert	-- 是否反向移行（解密时使用）。
*	输出参数：	pState	-- 移行后的状态数据。
*	返回值：	无。
*****************************************************************************/
void aes_128_decrypt::ShiftRows(unsigned char *pState, unsigned char bInvert)
{
    // 注意：状态数据以列形式存放！
    unsigned char r;	// row，   行
    unsigned char c;	// column，列
    unsigned char temp;
    unsigned char rowData[4];

    for (r = 1; r < 4; r++) {
        // 备份一行数据
        for (c = 0; c < 4; c++) {
            rowData[c] = pState[r + 4*c];
        }

        temp = bInvert ? (4 - r) : r;
        for (c = 0; c < 4; c++) {
            pState[r + 4*c] = rowData[(c + temp) % 4];
        }
    }
}

/*****************************************************************************
*	函数名：	GfMultBy02
*	功能描述：	在GF(28)域的 乘2 运算。
*	输入参数：	num	-- 乘数。
*	输出参数：	无。
*	返回值：	num乘以2的结果。
*****************************************************************************/
unsigned char aes_128_decrypt::GfMultBy02(unsigned char num)
{
    if (0 == (num & 0x80)) {
        num = num << 1;
    } else {
        num = (num << 1) ^ BPOLY;
    }
    return num;
}

//  GF(2^8) 乘法成员函数
unsigned char aes_128_decrypt::GfMultBy03(unsigned char num) {
    return GfMultBy02(num) ^ num;
}

unsigned char aes_128_decrypt::GfMultBy09(unsigned char num) {
    return GfMultBy02(GfMultBy02(GfMultBy02(num))) ^ num;
}

unsigned char aes_128_decrypt::GfMultBy0B(unsigned char num) {
    return GfMultBy02(GfMultBy02(GfMultBy02(num))) ^ GfMultBy02(num) ^ num;
}

unsigned char aes_128_decrypt::GfMultBy0D(unsigned char num) {
    return GfMultBy02(GfMultBy02(GfMultBy02(num))) ^ GfMultBy02(GfMultBy02(num)) ^ num;
}

unsigned char aes_128_decrypt::GfMultBy0E(unsigned char num) {
    return GfMultBy02(GfMultBy02(GfMultBy02(num))) ^ GfMultBy02(GfMultBy02(num)) ^ GfMultBy02(num);
}

/*****************************************************************************
*	函数名：	MixColumns
*	功能描述：	混合各列数据。
*	输入参数：	pData	 -- 数据。
*			   bInvert	-- 是否反向混合（解密时使用）。
*	输出参数：	pData	 -- 混合列后的状态数据。
*	返回值：	无。
*****************************************************************************/
void aes_128_decrypt::MixColumns(unsigned char *pData, unsigned char bInvert)
{
    if (bInvert) {

        for (unsigned char i = 0; i < 4; i++, pData += 4) {
            unsigned char a0 = pData[0], a1 = pData[1], a2 = pData[2], a3 = pData[3];
            pData[0] = GfMultBy0E(a0) ^ GfMultBy0B(a1) ^ GfMultBy0D(a2) ^ GfMultBy09(a3);
            pData[1] = GfMultBy09(a0) ^ GfMultBy0E(a1) ^ GfMultBy0B(a2) ^ GfMultBy0D(a3);
            pData[2] = GfMultBy0D(a0) ^ GfMultBy09(a1) ^ GfMultBy0E(a2) ^ GfMultBy0B(a3);
            pData[3] = GfMultBy0B(a0) ^ GfMultBy0D(a1) ^ GfMultBy09(a2) ^ GfMultBy0E(a3);
        }
    } else {

        for (unsigned char i = 0; i < 4; i++, pData += 4) {
            unsigned char temp = pData[0] ^ pData[1] ^ pData[2] ^ pData[3];
            unsigned char result[4];
            result[0] = temp ^ pData[0] ^ GfMultBy02((unsigned char)(pData[0] ^ pData[1]));
            result[1] = temp ^ pData[1] ^ GfMultBy02((unsigned char)(pData[1] ^ pData[2]));
            result[2] = temp ^ pData[2] ^ GfMultBy02((unsigned char)(pData[2] ^ pData[3]));
            result[3] = temp ^ pData[3] ^ GfMultBy02((unsigned char)(pData[3] ^ pData[0]));
            memcpy(pData, result, 4);
        }
    }
}

/*****************************************************************************
*	函数名：	BlockEncrypt
*	功能描述：	对单块数据加密。
*	输入参数：	pData -- 要加密的块数据。
*	输出参数：	pData -- 加密后的块数据。
*	返回值：	无。
*****************************************************************************/
void aes_128_decrypt::BlockEncrypt(AESInfo_t *aesInfoP, unsigned char *pData)
{
    unsigned char i;

    AddKey(pData, aesInfoP->expandKey);
    for (i = 1; i <= aesInfoP->Nr; i++) {
        SubstituteBytes(pData, 4 * Nb, SBox);
        ShiftRows(pData, 0);

        if (i != aesInfoP->Nr) {
            MixColumns(pData, 0);
        }

        AddKey(pData, &aesInfoP->expandKey[4*Nb*i]);
    }
}

/*****************************************************************************
*	函数名：	BlockDecrypt
*	功能描述：	对单块数据解密。
*	输入参数：	pData -- 要解密的数据。
*	输出参数：	pData -- 解密后的数据。
*	返回值：	无。
*****************************************************************************/
void aes_128_decrypt::BlockDecrypt(AESInfo_t *aesInfoP, unsigned char *pData)
{
    unsigned char i;

    AddKey(pData, &aesInfoP->expandKey[4*Nb*aesInfoP->Nr]);

    for (i = aesInfoP->Nr; i > 0; i--) {
        ShiftRows(pData, 1);
        SubstituteBytes(pData, 4 * Nb, InvSBox);
        AddKey(pData, &aesInfoP->expandKey[4*Nb*(i-1)]);

        if (1 != i) {
            MixColumns(pData, 1);
        }
    }
}

/*****************************************************************************
*	函数名：	AESAddPKCS7Padding
*	描述：		PKCS7 方式填充数据
*	输入参数：	data -- 后面最多预留16个字节空间用于存放填充值
*			   len --  数据的长度
*	输出参数：	data  -- 添加填充码后的数据
*	返回值：	填充后的长度
*****************************************************************************/
unsigned int aes_128_decrypt::AESAddPKCS7Padding(unsigned char *data, unsigned int len)
{
    unsigned int newLen;
    newLen = len + 16 - (len % 16);
    memset(&data[len], newLen-len, newLen-len);
    return newLen;
}

/*****************************************************************************
*	函数名：	AESDelPKCS7Padding
*	描述：		PKCS7Padding 填充密文解密后剔除填充值
*	输入参数：	pData -- 解密后的数据
*			   len --  数据的长度
*	输出参数：	pData  -- 删除填充码后的数据
*	返回值：	删除后的实际有效数据长度，为0表示传入的数据异常
*****************************************************************************/
unsigned int aes_128_decrypt::AESDelPKCS7Padding(unsigned char *pData, unsigned int len)
{
    if (0 != (len & 0x0F)) {//1组16字节，(0 != (len & 0x0F)说明不是16的倍数
        return 0;
    }
    if (pData[len - 1] > len) {
        return 0;
    }

    return len - pData[len - 1];
}

/*****************************************************************************
*	函数名：	AESInit
*	功能描述：	初始化
*	输入参数：	aesInfoP -- 用户需要填充
*	输出参数：	无。
*	返回值：	无。
*****************************************************************************/
void aes_128_decrypt::AESInit(AESInfo_t *aesInfoP)
{
    unsigned char i;
    unsigned char *pExpandKey;//扩展密钥
    unsigned char Rcon[4] = {0x01, 0x00, 0x00, 0x00};

    switch (aesInfoP->type) {
        case AES128:
            aesInfoP->Nr = 10;
            aesInfoP->Nk = 4;
            break;
        case AES192:
            aesInfoP->Nr = 12;
            aesInfoP->Nk = 6;
            break;
        case AES256:
            aesInfoP->Nr = 14;
            aesInfoP->Nk = 8;
            break;
        default:
            aesInfoP->Nr = 10;
            aesInfoP->Nk = 4;
            break;
    }

    //拓展密匙
    memcpy(aesInfoP->expandKey, aesInfoP->key, 4 * aesInfoP->Nk);//第一个是原始密匙，
    pExpandKey = &aesInfoP->expandKey[4*aesInfoP->Nk]; //拓展密匙AES128:10个、AES192:12个、AES256:14个
    for (i = aesInfoP->Nk; i < Nb*(aesInfoP->Nr + 1); pExpandKey += 4, i++) {
        memcpy(pExpandKey, pExpandKey - 4, 4);

        if (0 == i % aesInfoP->Nk) {
            RShiftWord(pExpandKey);
            SubstituteBytes(pExpandKey, 4, SBox);
            XorBytes(pExpandKey, Rcon, 4);

            Rcon[0] = GfMultBy02(Rcon[0]);
        } else if (6 < aesInfoP->Nk && i % aesInfoP->Nk == Nb) {
            SubstituteBytes(pExpandKey, 4, SBox);
        }

        XorBytes(pExpandKey, pExpandKey - 4 * aesInfoP->Nk, 4);
    }
}

/*****************************************************************************
*	函数名：	AESEncrypt
*	功能描述：	加密数据
*	输入参数：  aesInfoP    -- 包含key、加密方式等初始化信息
*              pPlainText   -- 要加密的明文数据，其长度为dataLen字节。
*			    dataLen	   -- 明文数据长度，以字节为单位
*	输出参数：	pCipherText	-- 加密后的数据
*	返回值：	解密后的数据长度。
*****************************************************************************/
unsigned int aes_128_decrypt::AESEncrypt(AESInfo_t *aesInfoP, const unsigned char *pPlainText,
                                         unsigned char *pCipherText, unsigned int dataLen)
{
    unsigned int i;
    const void *pIV;

    if (pPlainText != pCipherText) {
        memcpy(pCipherText, pPlainText, dataLen);
    }

    //必须是16的整倍数，不够的填充，pkcs7算法是缺n补n个n，比如13字节数据缺了3个，后面就补3个3;如果刚好是16的倍数，就填充16个16
    dataLen = AESAddPKCS7Padding(pCipherText, dataLen);

    pIV = aesInfoP->pIV;
    for (i = dataLen / (4 * Nb); i > 0 ; i--, pCipherText += 4 * Nb) {
        if (AES_MODE_CBC == aesInfoP->mode) {
            XorBytes(pCipherText, (const unsigned char*)pIV, 4 * Nb);
        }

        BlockEncrypt(aesInfoP, pCipherText);
        pIV = pCipherText;
    }
    return dataLen;
}

/*****************************************************************************
*	函数名：	AESDecrypt
*	功能描述：	解密数据
*	输入参数：	aesInfoP    -- 包含key、加密方式等初始化信息
*	            pCipherText	-- 要解密的密文
*			    dataLen	   -- 密文数据长度，以字节为单位，必须是整倍数，AES128:16倍数、AES192:24倍数、AES256:32倍数。
*  输出参数：  pPlainText  -- 解密出来的明文
*	返回值：	返回解密后的数据长度。
*****************************************************************************/
unsigned int aes_128_decrypt::AESDecrypt(AESInfo_t *aesInfoP, unsigned char *pPlainText, const unsigned char *pCipherText,
                 unsigned int dataLen)
{
    unsigned int i;
    unsigned char *pPlainTextBack = pPlainText;

    if (pPlainText != pCipherText) {
        memcpy(pPlainText, pCipherText, dataLen);
    }

    //当mode=AES_MODE_CBC时需要从最后一块数据开始解密
    pPlainText += dataLen - 4 * Nb;
    for (i = dataLen / (4 * Nb); i > 0 ; i--, pPlainText -= 4 * Nb) {
        BlockDecrypt(aesInfoP, pPlainText);
        if (AES_MODE_CBC == aesInfoP->mode) {
            //原来的第一块数据是初始变量加密的
            if (1 == i) {
                XorBytes(pPlainText, (const unsigned char*)aesInfoP->pIV, 4 * Nb);
            } else {
                XorBytes(pPlainText, pPlainText - 4 * Nb, 4 * Nb);
            }
        }
    }

    //因为数据需要16字节对齐，可能有填充数据，需要去除后面的填充数据
    return AESDelPKCS7Padding(pPlainTextBack, dataLen);
}

bool aes_128_decrypt::isprint(char c)
{
  bool bRight = false;
  if((c >='0' && c<='9') &&
     (c >='a' && c<='f') &&
     (c >='A' && c<= 'F'))
  {
      bRight = true;
  }
  return bRight;
}

void aes_128_decrypt::PrintData(const char *head, unsigned char *data, unsigned int len)
{
    unsigned int i;

    printf("%s, len:%u:\r\n", head, len);

    //按16进制打印出来
    printf("HEX:[");
    for (i=0; i<len; i++) {
        printf("%02X ", data[i]);
    }
    printf("]\r\n");

    //按ASCII码打印出来
    printf("ASCII:[");
    for (i=0; i<len; i++) {
        if (isprint(data[i])) {//可打印字符
            printf("'%c' ", data[i]);
        } else {
            printf("\\%02X ", data[i]);
        }

    }
    printf("]\r\n");
}

void aes_128_decrypt::my_aes_init(void)
{
  //初始化
  aesInfo.type = AES128;
  aesInfo.mode = AES_MODE_CBC;
  aesInfo.key = aes_key;
  aesInfo.pIV = aes_IV;

  AESInit(&aesInfo);
}

unsigned int aes_128_decrypt::my_aes_encrypt(unsigned char* sou_data, unsigned char* enc_data, unsigned int len)
{
    unsigned int enc_len;
    enc_len = AESEncrypt(&aesInfo, sou_data, enc_data, len);
    //PrintData("encryptMsg", enc_data, enc_len);
    return enc_len;
}

unsigned int aes_128_decrypt::my_aes_decrypt(unsigned char* enc_data, unsigned char* dec_data, unsigned int len)
{
    unsigned int dec_len;
    dec_len = AESDecrypt(&aesInfo, dec_data, enc_data, len);
    //PrintData("decryptMsg", dec_data, dec_len);
    return dec_len;
}

void aes_128_decrypt::getKey(unsigned char *pKey)
{
  memcpy(pKey, aes_key, sizeof(aes_key));
}

//测试例1
void aes_128_decrypt::my_aes_test(void)
{
    //要加密的内容
    unsigned char sourceMsg[8] = "hello";

    unsigned char encrypt_data[33]={0};
    unsigned char decrypt_data[33]={0};
    my_aes_init();
    PrintData("sourceMsg", sourceMsg, 5);
    my_aes_encrypt(sourceMsg,encrypt_data, 5);
    my_aes_decrypt(encrypt_data,decrypt_data, 16);
}
