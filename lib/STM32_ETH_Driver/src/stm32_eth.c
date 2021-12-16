/**
  ******************************************************************************
  * @file    stm32_eth.c
  * @author  MCD Application Team
  * @version V1.1.0
  * @date    11/20/2009
  * @brief   This file provides all the ETH firmware functions.
  ******************************************************************************
  * @copy
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2009 STMicroelectronics</center></h2>
  */ 

/* Includes ------------------------------------------------------------------*/
#include "platform.h"
#include "stm32_eth.h"

/** @addtogroup STM32_ETH_Driver
  * @brief ETH driver modules
  * @{
  */

/** @defgroup ETH_Private_TypesDefinitions
  * @{
  */ 
/**
  * @}
  */ 


/** @defgroup ETH_Private_Defines
  * @{
  */ 
/* Global pointers on Tx and Rx descriptor used to track transmit and receive descriptors */
ETH_DMADESCTypeDef  *DMATxDescToSet;
ETH_DMADESCTypeDef  *DMARxDescToGet;
ETH_DMADESCTypeDef  *DMAPTPTxDescToSet;
ETH_DMADESCTypeDef  *DMAPTPRxDescToGet;

/* ETHERNET MAC address offsets */
#define ETH_MAC_ADDR_HBASE   (ETH_MAC_BASE + 0x40)  /* ETHERNET MAC address high offset */
#define ETH_MAC_ADDR_LBASE    (ETH_MAC_BASE + 0x44)  /* ETHERNET MAC address low offset */

/* ETHERNET MACMIIAR register Mask */
#define MACMIIAR_CR_MASK    ((uint32_t)0xFFFFFFE3) 

/* ETHERNET MACCR register Mask */
#define MACCR_CLEAR_MASK    ((uint32_t)0xFF20810F)  

/* ETHERNET MACFCR register Mask */
#define MACFCR_CLEAR_MASK   ((uint32_t)0x0000FF41)

/* ETHERNET DMAOMR register Mask */
#define DMAOMR_CLEAR_MASK   ((uint32_t)0xF8DE3F23)

/* ETHERNET Remote Wake-up frame register length */
#define ETH_WAKEUP_REGISTER_LENGTH      8

/* ETHERNET Missed frames counter Shift */
#define  ETH_DMA_RX_OVERFLOW_MISSEDFRAMES_COUNTERSHIFT     17

/* ETHERNET DMA Tx descriptors Collision Count Shift */
#define  ETH_DMATXDESC_COLLISION_COUNTSHIFT        3

/* ETHERNET DMA Tx descriptors Buffer2 Size Shift */
#define  ETH_DMATXDESC_BUFFER2_SIZESHIFT           16

/* ETHERNET DMA Rx descriptors Frame Length Shift */
#define  ETH_DMARXDESC_FRAME_LENGTHSHIFT           16

/* ETHERNET DMA Rx descriptors Buffer2 Size Shift */
#define  ETH_DMARXDESC_BUFFER2_SIZESHIFT           16

/* ETHERNET errors */
#define  ETH_ERROR              ((uint32_t)0)
#define  ETH_SUCCESS            ((uint32_t)1)
/**
  * @}
  */

/** @defgroup ETH_Private_Macros
  * @{
  */ 
/**
  * @}
  */

/** @defgroup ETH_Private_Variables
  * @{
  */ 
/**
  * @}
  */

/** @defgroup ETH_Private_FunctionPrototypes
  * @{
  */ 
  
#ifndef USE_Delay
static void ETH_Delay(__IO uint32_t nCount);
#endif /* USE_Delay*/
  
/**
  * @}
  */

/** @defgroup ETH_Private_Functions
  * @{
  */

/**
  * @brief  Deinitializes the ETHERNET peripheral registers to their default reset values.
  * @param  None 
  * @retval None
  */
void ETH_DeInit(void)
{
#ifdef STM32F107RC
  RCC_AHBPeriphResetCmd(RCC_AHBPeriph_ETH_MAC, ENABLE);
  RCC_AHBPeriphResetCmd(RCC_AHBPeriph_ETH_MAC, DISABLE);
#endif
#ifdef STM32F417VG
  RCC_AHB1PeriphResetCmd(RCC_AHB1Periph_ETH_MAC, ENABLE);
  RCC_AHB1PeriphResetCmd(RCC_AHB1Periph_ETH_MAC, DISABLE);
#endif
}

/**
  * @brief  Initializes the ETHERNET peripheral according to the specified
  *   parameters in the ETH_InitStruct .
  * @param ETH_InitStruct: pointer to a ETH_InitTypeDef structure that contains
  *   the configuration information for the specified ETHERNET peripheral.
  * @param PHYAddress: external PHY address                    
  * @retval ETH_ERROR: Ethernet initialization failed
  *         ETH_SUCCESS: Ethernet successfully initialized                 
  */
uint32_t ETH_Init(ETH_InitTypeDef* ETH_InitStruct, uint16_t PHYAddress)
{
	uint32_t tmpreg = 0;
  uint32_t hclk = 60000000;

  /* Check the parameters */
  /* MAC --------------------------*/ 
  assert_param(IS_ETH_AUTONEGOTIATION(ETH_InitStruct->ETH_AutoNegotiation));
  assert_param(IS_ETH_WATCHDOG(ETH_InitStruct->ETH_Watchdog));
  assert_param(IS_ETH_JABBER(ETH_InitStruct->ETH_Jabber));
  assert_param(IS_ETH_INTER_FRAME_GAP(ETH_InitStruct->ETH_InterFrameGap));
  assert_param(IS_ETH_CARRIER_SENSE(ETH_InitStruct->ETH_CarrierSense));
  assert_param(IS_ETH_SPEED(ETH_InitStruct->ETH_Speed));
  assert_param(IS_ETH_RECEIVE_OWN(ETH_InitStruct->ETH_ReceiveOwn));
  assert_param(IS_ETH_LOOPBACK_MODE(ETH_InitStruct->ETH_LoopbackMode));
  assert_param(IS_ETH_DUPLEX_MODE(ETH_InitStruct->ETH_Mode));
  assert_param(IS_ETH_CHECKSUM_OFFLOAD(ETH_InitStruct->ETH_ChecksumOffload));
  assert_param(IS_ETH_RETRY_TRANSMISSION(ETH_InitStruct->ETH_RetryTransmission));
  assert_param(IS_ETH_AUTOMATIC_PADCRC_STRIP(ETH_InitStruct->ETH_AutomaticPadCRCStrip));
  assert_param(IS_ETH_BACKOFF_LIMIT(ETH_InitStruct->ETH_BackOffLimit));
  assert_param(IS_ETH_DEFERRAL_CHECK(ETH_InitStruct->ETH_DeferralCheck));
  assert_param(IS_ETH_RECEIVE_ALL(ETH_InitStruct->ETH_ReceiveAll));
  assert_param(IS_ETH_SOURCE_ADDR_FILTER(ETH_InitStruct->ETH_SourceAddrFilter));
  assert_param(IS_ETH_CONTROL_FRAMES(ETH_InitStruct->ETH_PassControlFrames));
  assert_param(IS_ETH_BROADCAST_FRAMES_RECEPTION(ETH_InitStruct->ETH_BroadcastFramesReception));
  assert_param(IS_ETH_DESTINATION_ADDR_FILTER(ETH_InitStruct->ETH_DestinationAddrFilter));
  assert_param(IS_ETH_PROMISCUOUS_MODE(ETH_InitStruct->ETH_PromiscuousMode));
  assert_param(IS_ETH_MULTICAST_FRAMES_FILTER(ETH_InitStruct->ETH_MulticastFramesFilter));  
  assert_param(IS_ETH_UNICAST_FRAMES_FILTER(ETH_InitStruct->ETH_UnicastFramesFilter));
  assert_param(IS_ETH_PAUSE_TIME(ETH_InitStruct->ETH_PauseTime));
  assert_param(IS_ETH_ZEROQUANTA_PAUSE(ETH_InitStruct->ETH_ZeroQuantaPause));
  assert_param(IS_ETH_PAUSE_LOW_THRESHOLD(ETH_InitStruct->ETH_PauseLowThreshold));
  assert_param(IS_ETH_UNICAST_PAUSE_FRAME_DETECT(ETH_InitStruct->ETH_UnicastPauseFrameDetect));
  assert_param(IS_ETH_RECEIVE_FLOWCONTROL(ETH_InitStruct->ETH_ReceiveFlowControl));
  assert_param(IS_ETH_TRANSMIT_FLOWCONTROL(ETH_InitStruct->ETH_TransmitFlowControl));
  assert_param(IS_ETH_VLAN_TAG_COMPARISON(ETH_InitStruct->ETH_VLANTagComparison));
  assert_param(IS_ETH_VLAN_TAG_IDENTIFIER(ETH_InitStruct->ETH_VLANTagIdentifier));
  /* DMA --------------------------*/
  assert_param(IS_ETH_DROP_TCPIP_CHECKSUM_FRAME(ETH_InitStruct->ETH_DropTCPIPChecksumErrorFrame));
  assert_param(IS_ETH_RECEIVE_STORE_FORWARD(ETH_InitStruct->ETH_ReceiveStoreForward));
  assert_param(IS_ETH_FLUSH_RECEIVE_FRAME(ETH_InitStruct->ETH_FlushReceivedFrame));
  assert_param(IS_ETH_TRANSMIT_STORE_FORWARD(ETH_InitStruct->ETH_TransmitStoreForward));
  assert_param(IS_ETH_TRANSMIT_THRESHOLD_CONTROL(ETH_InitStruct->ETH_TransmitThresholdControl));
  assert_param(IS_ETH_FORWARD_ERROR_FRAMES(ETH_InitStruct->ETH_ForwardErrorFrames));
  assert_param(IS_ETH_FORWARD_UNDERSIZED_GOOD_FRAMES(ETH_InitStruct->ETH_ForwardUndersizedGoodFrames));
  assert_param(IS_ETH_RECEIVE_THRESHOLD_CONTROL(ETH_InitStruct->ETH_ReceiveThresholdControl));
  assert_param(IS_ETH_SECOND_FRAME_OPERATE(ETH_InitStruct->ETH_SecondFrameOperate));
  assert_param(IS_ETH_ADDRESS_ALIGNED_BEATS(ETH_InitStruct->ETH_AddressAlignedBeats));
  assert_param(IS_ETH_FIXED_BURST(ETH_InitStruct->ETH_FixedBurst));
  assert_param(IS_ETH_RXDMA_BURST_LENGTH(ETH_InitStruct->ETH_RxDMABurstLength));
  assert_param(IS_ETH_TXDMA_BURST_LENGTH(ETH_InitStruct->ETH_TxDMABurstLength)); 
  assert_param(IS_ETH_DMA_DESC_SKIP_LENGTH(ETH_InitStruct->ETH_DescriptorSkipLength));  
  assert_param(IS_ETH_DMA_ARBITRATION_ROUNDROBIN_RXTX(ETH_InitStruct->ETH_DMAArbitration));       
  /*-------------------------------- MAC Config ------------------------------*/   
  /*---------------------- ETHERNET MACMIIAR Configuration -------------------*/
  /* Get the ETHERNET MACMIIAR value */
  tmpreg = ETH->MACMIIAR;
  /* Clear CSR Clock Range CR[2:0] bits */
  tmpreg &= MACMIIAR_CR_MASK;

  /* Get hclk frequency value */
  hclk = SystemFrequency_AHBClk;
  /* Set CR bits depending on hclk value */
  if((hclk >= 20000000)&&(hclk < 35000000))
  {
    /* CSR Clock Range between 20-35 MHz */
    tmpreg |= (uint32_t)ETH_MACMIIAR_CR_DIV16;
  }
  else if((hclk >= 35000000)&&(hclk < 60000000))
  {
    /* CSR Clock Range between 35-60 MHz */
    tmpreg |= (uint32_t)ETH_MACMIIAR_CR_DIV26;
  }  
  else if((hclk >= 60000000)&&(hclk < 100000000))
  {
    /* CSR Clock Range between 60-100 MHz */
    tmpreg |= (uint32_t)ETH_MACMIIAR_CR_DIV42;
  }
#if defined(STM32F4XX)
  else if((hclk >= 100000000)&&(hclk < 150000000))
  {
    /* CSR Clock Range between 100-150 MHz */
    tmpreg |= (uint32_t)ETH_MACMIIAR_CR_Div62;
  }
  else /* ((hclk >= 150000000)&&(hclk <= 168000000)) */
  {
    /* CSR Clock Range between 150-168 MHz */
    tmpreg |= (uint32_t)ETH_MACMIIAR_CR_Div102;
  }
#endif
  /* Write to ETHERNET MAC MIIAR: Configure the ETHERNET CSR Clock Range */
  ETH->MACMIIAR = (uint32_t)tmpreg;
  /*-------------------- PHY initialization and configuration ----------------*/
  /* Put the PHY in reset mode */
  if(!(ETH_WritePHYRegister(PHYAddress, PHY_BCR, PHY_Reset)))
  {
    /* Return ERROR in case of write timeout */
    return ETH_ERROR;
  }
  
  /* Delay to assure PHY reset */
  _eth_delay_(PHY_ResetDelay);
  
    
  if(ETH_InitStruct->ETH_AutoNegotiation != ETH_AutoNegotiation_Disable)
  {  
//    uint32_t RegValue = 0;
//    __IO uint32_t timeout = 0;
//
//    /* We wait for linked satus... */
//    do
//    {
//      timeout++;
//    } while (!(ETH_ReadPHYRegister(PHYAddress, PHY_BSR) & PHY_Linked_Status) && (timeout < PHY_READ_TO));
//    /* Return ERROR in case of timeout */
//    if(timeout == PHY_READ_TO)
//    {
//      return ETH_ERROR;
//    }
//    /* Reset Timeout counter */
//    timeout = 0;
//    
//    /* Enable Auto-Negotiation */
//    if(!(ETH_WritePHYRegister(PHYAddress, PHY_BCR, PHY_AutoNegotiation)))
//    {
//      /* Return ERROR in case of write timeout */
//      return ETH_ERROR;
//    }
//    
//    /* Wait until the autonegotiation will be completed */
//    do
//    {
//      timeout++;
//    } while (!(ETH_ReadPHYRegister(PHYAddress, PHY_BSR) & PHY_AutoNego_Complete) && (timeout < (uint32_t)PHY_READ_TO));  
//    /* Return ERROR in case of timeout */
//    if(timeout == PHY_READ_TO)
//    {
//      return ETH_ERROR;
//    }
//    /* Reset Timeout counter */
//    timeout = 0;
//    
//    /* Read the result of the autonegotiation */
//    RegValue = ETH_ReadPHYRegister(PHYAddress, PHY_SR);
//  
//    /* Configure the MAC with the Duplex Mode fixed by the autonegotiation process */
//    if((RegValue & PHY_DUPLEX_STATUS) != (uint32_t)RESET)
//    {
//      /* Set Ethernet duplex mode to FullDuplex following the autonegotiation */
//      ETH_InitStruct->ETH_Mode = ETH_Mode_FullDuplex;
//            
//    }
//    else
//    {
//      /* Set Ethernet duplex mode to HalfDuplex following the autonegotiation */
//      ETH_InitStruct->ETH_Mode = ETH_Mode_HalfDuplex;           
//    }
//    /* Configure the MAC with the speed fixed by the autonegotiation process */
//    if(RegValue & PHY_SPEED_STATUS)
//    {  
//      /* Set Ethernet speed to 10M following the autonegotiation */    
//      ETH_InitStruct->ETH_Speed = ETH_Speed_10M; 
//    }
//    else
//    {   
//      /* Set Ethernet speed to 100M following the autonegotiation */ 
//      ETH_InitStruct->ETH_Speed = ETH_Speed_100M;      
//    }    
  }
  else
  {
    if(!ETH_WritePHYRegister(PHYAddress, PHY_BCR, ((uint16_t)(ETH_InitStruct->ETH_Mode >> 3) |
                                                   (uint16_t)(ETH_InitStruct->ETH_Speed >> 1))))
    {
      /* Return ERROR in case of write timeout */
      return ETH_ERROR;
    }
    /* Delay to assure PHY configuration */
    _eth_delay_(PHY_ConfigDelay);
    
  }
  /*------------------------ ETHERNET MACCR Configuration --------------------*/
  /* Get the ETHERNET MACCR value */  
  tmpreg = ETH->MACCR;
  /* Clear WD, PCE, PS, TE and RE bits */
  tmpreg &= MACCR_CLEAR_MASK;
  /* Set the WD bit according to ETH_Watchdog value */
  /* Set the JD: bit according to ETH_Jabber value */
  /* Set the IFG bit according to ETH_InterFrameGap value */ 
  /* Set the DCRS bit according to ETH_CarrierSense value */  
  /* Set the FES bit according to ETH_Speed value */ 
  /* Set the DO bit according to ETH_ReceiveOwn value */ 
  /* Set the LM bit according to ETH_LoopbackMode value */ 
  /* Set the DM bit according to ETH_Mode value */ 
  /* Set the IPC bit according to ETH_ChecksumOffload value */                   
  /* Set the DR bit according to ETH_RetryTransmission value */ 
  /* Set the ACS bit according to ETH_AutomaticPadCRCStrip value */ 
  /* Set the BL bit according to ETH_BackOffLimit value */ 
  /* Set the DC bit according to ETH_DeferralCheck value */                          
  tmpreg |= (uint32_t)(ETH_InitStruct->ETH_Watchdog | 
                  ETH_InitStruct->ETH_Jabber | 
                  ETH_InitStruct->ETH_InterFrameGap |
                  ETH_InitStruct->ETH_CarrierSense |
                  ETH_InitStruct->ETH_Speed | 
                  ETH_InitStruct->ETH_ReceiveOwn |
                  ETH_InitStruct->ETH_LoopbackMode |
                  ETH_InitStruct->ETH_Mode | 
                  ETH_InitStruct->ETH_ChecksumOffload |    
                  ETH_InitStruct->ETH_RetryTransmission | 
                  ETH_InitStruct->ETH_AutomaticPadCRCStrip | 
                  ETH_InitStruct->ETH_BackOffLimit | 
                  ETH_InitStruct->ETH_DeferralCheck);
  /* Write to ETHERNET MACCR */
  ETH->MACCR = (uint32_t)tmpreg;
  
  /*----------------------- ETHERNET MACFFR Configuration --------------------*/ 
  /* Set the RA bit according to ETH_ReceiveAll value */
  /* Set the SAF and SAIF bits according to ETH_SourceAddrFilter value */
  /* Set the PCF bit according to ETH_PassControlFrames value */
  /* Set the DBF bit according to ETH_BroadcastFramesReception value */
  /* Set the DAIF bit according to ETH_DestinationAddrFilter value */
  /* Set the PR bit according to ETH_PromiscuousMode value */
  /* Set the PM, HMC and HPF bits according to ETH_MulticastFramesFilter value */
  /* Set the HUC and HPF bits according to ETH_UnicastFramesFilter value */
  /* Write to ETHERNET MACFFR */  
  ETH->MACFFR = (uint32_t)(ETH_InitStruct->ETH_ReceiveAll | 
                          ETH_InitStruct->ETH_SourceAddrFilter |
                          ETH_InitStruct->ETH_PassControlFrames |
                          ETH_InitStruct->ETH_BroadcastFramesReception | 
                          ETH_InitStruct->ETH_DestinationAddrFilter |
                          ETH_InitStruct->ETH_PromiscuousMode |
                          ETH_InitStruct->ETH_MulticastFramesFilter |
                          ETH_InitStruct->ETH_UnicastFramesFilter); 
  /*--------------- ETHERNET MACHTHR and MACHTLR Configuration ---------------*/
  /* Write to ETHERNET MACHTHR */
  ETH->MACHTHR = (uint32_t)ETH_InitStruct->ETH_HashTableHigh;
  /* Write to ETHERNET MACHTLR */
  ETH->MACHTLR = (uint32_t)ETH_InitStruct->ETH_HashTableLow;
  /*----------------------- ETHERNET MACFCR Configuration --------------------*/
  /* Get the ETHERNET MACFCR value */  
  tmpreg = ETH->MACFCR;
  /* Clear xx bits */
  tmpreg &= MACFCR_CLEAR_MASK;
  
  /* Set the PT bit according to ETH_PauseTime value */
  /* Set the DZPQ bit according to ETH_ZeroQuantaPause value */
  /* Set the PLT bit according to ETH_PauseLowThreshold value */
  /* Set the UP bit according to ETH_UnicastPauseFrameDetect value */
  /* Set the RFE bit according to ETH_ReceiveFlowControl value */
  /* Set the TFE bit according to ETH_TransmitFlowControl value */  
  tmpreg |= (uint32_t)((ETH_InitStruct->ETH_PauseTime << 16) | 
                   ETH_InitStruct->ETH_ZeroQuantaPause |
                   ETH_InitStruct->ETH_PauseLowThreshold |
                   ETH_InitStruct->ETH_UnicastPauseFrameDetect | 
                   ETH_InitStruct->ETH_ReceiveFlowControl |
                   ETH_InitStruct->ETH_TransmitFlowControl); 
  /* Write to ETHERNET MACFCR */
  ETH->MACFCR = (uint32_t)tmpreg;
  /*----------------------- ETHERNET MACVLANTR Configuration -----------------*/
  /* Set the ETV bit according to ETH_VLANTagComparison value */
  /* Set the VL bit according to ETH_VLANTagIdentifier value */  
  ETH->MACVLANTR = (uint32_t)(ETH_InitStruct->ETH_VLANTagComparison | 
                             ETH_InitStruct->ETH_VLANTagIdentifier); 
       
  /*-------------------------------- DMA Config ------------------------------*/
  /*----------------------- ETHERNET DMAOMR Configuration --------------------*/
  /* Get the ETHERNET DMAOMR value */  
  tmpreg = ETH->DMAOMR;
  /* Clear xx bits */
  tmpreg &= DMAOMR_CLEAR_MASK;
  
  /* Set the DT bit according to ETH_DropTCPIPChecksumErrorFrame value */
  /* Set the RSF bit according to ETH_ReceiveStoreForward value */
  /* Set the DFF bit according to ETH_FlushReceivedFrame value */
  /* Set the TSF bit according to ETH_TransmitStoreForward value */
  /* Set the TTC bit according to ETH_TransmitThresholdControl value */
  /* Set the FEF bit according to ETH_ForwardErrorFrames value */
  /* Set the FUF bit according to ETH_ForwardUndersizedGoodFrames value */
  /* Set the RTC bit according to ETH_ReceiveThresholdControl value */
  /* Set the OSF bit according to ETH_SecondFrameOperate value */
  tmpreg |= (uint32_t)(ETH_InitStruct->ETH_DropTCPIPChecksumErrorFrame | 
                  ETH_InitStruct->ETH_ReceiveStoreForward |
                  ETH_InitStruct->ETH_FlushReceivedFrame |
                  ETH_InitStruct->ETH_TransmitStoreForward | 
                  ETH_InitStruct->ETH_TransmitThresholdControl |
                  ETH_InitStruct->ETH_ForwardErrorFrames |
                  ETH_InitStruct->ETH_ForwardUndersizedGoodFrames |
                  ETH_InitStruct->ETH_ReceiveThresholdControl |                                   
                  ETH_InitStruct->ETH_SecondFrameOperate); 
  /* Write to ETHERNET DMAOMR */
  ETH->DMAOMR = (uint32_t)tmpreg;
  
  /*----------------------- ETHERNET DMABMR Configuration --------------------*/ 
  /* Set the AAL bit according to ETH_AddressAlignedBeats value */
  /* Set the FB bit according to ETH_FixedBurst value */
  /* Set the RPBL and 4*PBL bits according to ETH_RxDMABurstLength value */
  /* Set the PBL and 4*PBL bits according to ETH_TxDMABurstLength value */
  /* Set the DSL bit according to ETH_DesciptorSkipLength value */
  /* Set the PR and DA bits according to ETH_DMAArbitration value */         
  ETH->DMABMR = (uint32_t)(ETH_InitStruct->ETH_AddressAlignedBeats | 
                          ETH_InitStruct->ETH_FixedBurst |
                          ETH_InitStruct->ETH_RxDMABurstLength | /* !! if 4xPBL is selected for Tx or Rx it is applied for the other */
                          ETH_InitStruct->ETH_TxDMABurstLength | 
                         (ETH_InitStruct->ETH_DescriptorSkipLength << 2) |
                          ETH_InitStruct->ETH_DMAArbitration |
                          ETH_DMABMR_USP); /* Enable use of separate PBL for Rx and Tx */  
  /* Return Ethernet configuration success */
  return ETH_SUCCESS;
}

/**
  * @brief  Fills each ETH_InitStruct member with its default value.
  * @param  ETH_InitStruct: pointer to a ETH_InitTypeDef structure which will be initialized.
  * @retval None
  */
void ETH_StructInit(ETH_InitTypeDef* ETH_InitStruct)
{
  /* ETH_InitStruct members default value */
  /*------------------------   MAC   -----------------------------------*/     
  ETH_InitStruct->ETH_AutoNegotiation = ETH_AutoNegotiation_Disable;           
  ETH_InitStruct->ETH_Watchdog = ETH_Watchdog_Enable;                   
  ETH_InitStruct->ETH_Jabber = ETH_Jabber_Enable;                                                     
  ETH_InitStruct->ETH_InterFrameGap = ETH_InterFrameGap_96Bit;                                                                                                                          
  ETH_InitStruct->ETH_CarrierSense = ETH_CarrierSense_Enable;                              
  ETH_InitStruct->ETH_Speed = ETH_Speed_10M;                       
  ETH_InitStruct->ETH_ReceiveOwn = ETH_ReceiveOwn_Enable;               
  ETH_InitStruct->ETH_LoopbackMode = ETH_LoopbackMode_Disable;              
  ETH_InitStruct->ETH_Mode = ETH_Mode_HalfDuplex;                       
  ETH_InitStruct->ETH_ChecksumOffload = ETH_ChecksumOffload_Disable;                                                            
  ETH_InitStruct->ETH_RetryTransmission = ETH_RetryTransmission_Enable;                                                                                  
  ETH_InitStruct->ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;                                                         
  ETH_InitStruct->ETH_BackOffLimit = ETH_BackOffLimit_10;                                                                
  ETH_InitStruct->ETH_DeferralCheck = ETH_DeferralCheck_Disable;                                                                                                                  
  ETH_InitStruct->ETH_ReceiveAll = ETH_ReceiveAll_Disable;                                                               
  ETH_InitStruct->ETH_SourceAddrFilter = ETH_SourceAddrFilter_Disable;                                
  ETH_InitStruct->ETH_PassControlFrames = ETH_PassControlFrames_BlockAll;          
  ETH_InitStruct->ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Disable;
  ETH_InitStruct->ETH_DestinationAddrFilter = ETH_DestinationAddrFilter_Normal;      
  ETH_InitStruct->ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;                                                             
  ETH_InitStruct->ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;      
  ETH_InitStruct->ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;      
  ETH_InitStruct->ETH_HashTableHigh = 0x0;                
  ETH_InitStruct->ETH_HashTableLow = 0x0;                     
  ETH_InitStruct->ETH_PauseTime = 0x0;                 
  ETH_InitStruct->ETH_ZeroQuantaPause = ETH_ZeroQuantaPause_Disable;            
  ETH_InitStruct->ETH_PauseLowThreshold = ETH_PauseLowThreshold_Minus4;         
  ETH_InitStruct->ETH_UnicastPauseFrameDetect = ETH_UnicastPauseFrameDetect_Disable;   
  ETH_InitStruct->ETH_ReceiveFlowControl = ETH_ReceiveFlowControl_Disable;        
  ETH_InitStruct->ETH_TransmitFlowControl = ETH_TransmitFlowControl_Disable;      
  ETH_InitStruct->ETH_VLANTagComparison = ETH_VLANTagComparison_16Bit;          
  ETH_InitStruct->ETH_VLANTagIdentifier = 0x0;          
  /*------------------------   DMA   -----------------------------------*/  
  ETH_InitStruct->ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Disable; 
  ETH_InitStruct->ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;       
  ETH_InitStruct->ETH_FlushReceivedFrame = ETH_FlushReceivedFrame_Disable;       
  ETH_InitStruct->ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;     
  ETH_InitStruct->ETH_TransmitThresholdControl = ETH_TransmitThresholdControl_64Bytes;  
  ETH_InitStruct->ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;       
  ETH_InitStruct->ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Disable; 
  ETH_InitStruct->ETH_ReceiveThresholdControl = ETH_ReceiveThresholdControl_64Bytes;   
  ETH_InitStruct->ETH_SecondFrameOperate = ETH_SecondFrameOperate_Disable;
  ETH_InitStruct->ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;
  ETH_InitStruct->ETH_FixedBurst = ETH_FixedBurst_Disable;
  ETH_InitStruct->ETH_RxDMABurstLength = ETH_RxDMABurstLength_1Beat;
  ETH_InitStruct->ETH_TxDMABurstLength = ETH_TxDMABurstLength_1Beat;
  ETH_InitStruct->ETH_DescriptorSkipLength = 0x0;
  ETH_InitStruct->ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_1_1;
}

/**
  * @brief  Enables ENET MAC and DMA reception/transmission 
  * @param  None
  * @retval None
  */
void ETH_Start(void)
{
	ETH->MACCR |= (ETH_MACCR_TE | ETH_MACCR_RE);
	ETH->DMAOMR |= (ETH_DMAOMR_SR | ETH_DMAOMR_ST);
}

void ETH_Stop(void)
{
	ETH->DMAOMR &= ~(ETH_DMAOMR_SR | ETH_DMAOMR_ST);
	ETH->MACCR &= ~(ETH_MACCR_TE | ETH_MACCR_RE);
}

/**
  * @brief  Transmits a packet, from application buffer, pointed by ppkt.
  * @param  ppkt: pointer to the application's packet buffer to transmit.
  * @param  FrameLength: Tx Packet size.
  * @retval ETH_ERROR: in case of Tx desc owned by DMA
  *         ETH_SUCCESS: for correct transmission
  */
uint32_t ETH_HandleTxPkt(uint8_t *ppkt, uint16_t FrameLength)
{ 
  uint32_t offset = 0;
    
  /* Check if the descriptor is owned by the ETHERNET DMA (when set) or CPU (when reset) */
  if((DMATxDescToSet->Status & ETH_DMATxDesc_OWN) != (uint32_t)RESET)
  {
    /* Return ERROR: OWN bit set */
    return ETH_ERROR;
  }
  
  /* Copy the frame to be sent into memory pointed by the current ETHERNET DMA Tx descriptor */      
  for(offset=0; offset<FrameLength; offset++)       
  {
    (*(__IO uint8_t *)((DMATxDescToSet->Buffer1Addr) + offset)) = (*(ppkt + offset));
  }
        
  /* Setting the Frame Length: bits[12:0] */
  DMATxDescToSet->ControlBufferSize = (FrameLength & ETH_DMATxDesc_TBS1);
  /* Setting the last segment and first segment bits (in this case a frame is transmitted in one descriptor) */    
  DMATxDescToSet->Status |= ETH_DMATxDesc_LS | ETH_DMATxDesc_FS;
  /* Set Own bit of the Tx descriptor Status: gives the buffer back to ETHERNET DMA */
  DMATxDescToSet->Status |= ETH_DMATxDesc_OWN;
  /* When Tx Buffer unavailable flag is set: clear it and resume transmission */
  if ((ETH->DMASR & ETH_DMASR_TBUS) != (uint32_t)RESET)
  {
    /* Clear TBUS ETHERNET DMA flag */
    ETH->DMASR = ETH_DMASR_TBUS;
    /* Resume DMA transmission*/
    ETH->DMATPDR = 0;
  }
  
  /* Update the ETHERNET DMA global Tx descriptor with next Tx decriptor */  
  /* Chained Mode */
  if((DMATxDescToSet->Status & ETH_DMATxDesc_TCH) != (uint32_t)RESET)
  {     
    /* Selects the next DMA Tx descriptor list for next buffer to send */ 
    DMATxDescToSet = (ETH_DMADESCTypeDef*) (DMATxDescToSet->Buffer2NextDescAddr);    
  }
  else /* Ring Mode */
  {  
    if((DMATxDescToSet->Status & ETH_DMATxDesc_TER) != (uint32_t)RESET)
    {
      /* Selects the first DMA Tx descriptor for next buffer to send: last Tx descriptor was used */
      DMATxDescToSet = (ETH_DMADESCTypeDef*) (ETH->DMATDLAR);      
    }
    else
    {  
      /* Selects the next DMA Tx descriptor list for next buffer to send */
      DMATxDescToSet = (ETH_DMADESCTypeDef*) ((uint32_t)DMATxDescToSet + 0x10 + ((ETH->DMABMR & ETH_DMABMR_DSL) >> 2));      
    }
  }
  /* Return SUCCESS */
  return ETH_SUCCESS;   
}

/**
  * @brief  Receives a packet and copies it to memory pointed by ppkt.
  * @param  ppkt: pointer to the application packet receive buffer.
  * @retval ETH_ERROR: if there is error in reception
  *         framelength: received packet size if packet reception is correct
  */
uint32_t ETH_HandleRxPkt(uint8_t *ppkt)
{ 
  uint32_t offset = 0, framelength = 0;
  /* Check if the descriptor is owned by the ETHERNET DMA (when set) or CPU (when reset) */
  if((DMARxDescToGet->Status & ETH_DMARxDesc_OWN) != (uint32_t)RESET)
  {
    /* Return error: OWN bit set */
    return ETH_ERROR; 
  }
  
  if(((DMARxDescToGet->Status & ETH_DMARxDesc_ES) == (uint32_t)RESET) && 
     ((DMARxDescToGet->Status & ETH_DMARxDesc_LS) != (uint32_t)RESET) &&  
     ((DMARxDescToGet->Status & ETH_DMARxDesc_FS) != (uint32_t)RESET))  
  {      
    /* Get the Frame Length of the received packet: substruct 4 bytes of the CRC */
    framelength = ((DMARxDescToGet->Status & ETH_DMARxDesc_FL) >> ETH_DMARXDESC_FRAME_LENGTHSHIFT) - 4;
    /* Copy the received frame into buffer from memory pointed by the current ETHERNET DMA Rx descriptor */
    for(offset=0; offset<framelength; offset++)       
    {
      (*(ppkt + offset)) = (*(__IO uint8_t *)((DMARxDescToGet->Buffer1Addr) + offset));
    }
  }
  else
  {
    /* Return ERROR */
    framelength = ETH_ERROR;
  }
  /* Set Own bit of the Rx descriptor Status: gives the buffer back to ETHERNET DMA */
  DMARxDescToGet->Status = ETH_DMARxDesc_OWN; 
 
  /* When Rx Buffer unavailable flag is set: clear it and resume reception */
  if ((ETH->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET)  
  {
    /* Clear RBUS ETHERNET DMA flag */
    ETH->DMASR = ETH_DMASR_RBUS;
    /* Resume DMA reception */
    ETH->DMARPDR = 0;
  }
  
  /* Update the ETHERNET DMA global Rx descriptor with next Rx decriptor */      
  /* Chained Mode */
  if((DMARxDescToGet->ControlBufferSize & ETH_DMARxDesc_RCH) != (uint32_t)RESET)
  {     
    /* Selects the next DMA Rx descriptor list for next buffer to read */ 
    DMARxDescToGet = (ETH_DMADESCTypeDef*) (DMARxDescToGet->Buffer2NextDescAddr);    
  }
  else /* Ring Mode */
  {   
    if((DMARxDescToGet->ControlBufferSize & ETH_DMARxDesc_RER) != (uint32_t)RESET)
    {
      /* Selects the first DMA Rx descriptor for next buffer to read: last Rx descriptor was used */
      DMARxDescToGet = (ETH_DMADESCTypeDef*) (ETH->DMARDLAR);      
    }
    else
    { 
      /* Selects the next DMA Rx descriptor list for next buffer to read */
      DMARxDescToGet = (ETH_DMADESCTypeDef*) ((uint32_t)DMARxDescToGet + 0x10 + ((ETH->DMABMR & ETH_DMABMR_DSL) >> 2));      
    }
  }
  
  /* Return Frame Length/ERROR */
  return (framelength);  
}

/**
  * @brief  Get the size of received the received packet.
  * @param  None
  * @retval framelength: received packet size 
  */
uint32_t ETH_GetRxPktSize(void)
{
  uint32_t frameLength = 0;
  if(((DMARxDescToGet->Status & ETH_DMARxDesc_OWN) == (uint32_t)RESET) &&
     ((DMARxDescToGet->Status & ETH_DMARxDesc_ES) == (uint32_t)RESET) &&
     ((DMARxDescToGet->Status & ETH_DMARxDesc_LS) != (uint32_t)RESET) &&
     ((DMARxDescToGet->Status & ETH_DMARxDesc_FS) != (uint32_t)RESET))
  {
    /* Get the size of the packet: including 4 bytes of the CRC */
    frameLength = ETH_GetDMARxDescFrameLength(DMARxDescToGet);
  }
 
 /* Return Frame Length */ 
 return frameLength;
}

/**
  * @brief  Drop a Received packet (too small packet, etc...)
  * @param  None
  * @retval None
  */
void ETH_DropRxPkt(void)
{
  /* Set Own bit of the Rx descriptor Status: gives the buffer back to ETHERNET DMA */
  DMARxDescToGet->Status = ETH_DMARxDesc_OWN;  
  /* Chained Mode */
  if((DMARxDescToGet->ControlBufferSize & ETH_DMARxDesc_RCH) != (uint32_t)RESET)
  {
    /* Selects the next DMA Rx descriptor list for next buffer read */
    DMARxDescToGet = (ETH_DMADESCTypeDef*) (DMARxDescToGet->Buffer2NextDescAddr);
  }
  else /* Ring Mode */
  {
    if((DMARxDescToGet->ControlBufferSize & ETH_DMARxDesc_RER) != (uint32_t)RESET)
    {
      /* Selects the next DMA Rx descriptor list for next buffer read: this will
         be the first Rx descriptor in this case */
      DMARxDescToGet = (ETH_DMADESCTypeDef*) (ETH->DMARDLAR);
    }
    else
    {
      /* Selects the next DMA Rx descriptor list for next buffer read */
      DMARxDescToGet = (ETH_DMADESCTypeDef*) ((uint32_t)DMARxDescToGet + 0x10 + ((ETH->DMABMR & ETH_DMABMR_DSL) >> 2));
    }
  }
}

/*---------------------------------  PHY  ------------------------------------*/
/**
  * @brief  Read a PHY register
  * @param PHYAddress: PHY device address, is the index of one of supported 32 PHY devices. 
  *   This parameter can be one of the following values: 0,..,31                  
  * @param PHYReg: PHY register address, is the index of one of the 32 PHY register. 
  *   This parameter can be one of the following values: 
  *     @arg PHY_BCR: Tranceiver Basic Control Register 
  *     @arg PHY_BSR: Tranceiver Basic Status Register 
  *     @arg PHY_SR : Tranceiver Status Register    
  *     @arg More PHY register could be read depending on the used PHY
  * @retval ETH_ERROR: in case of timeout
  *         MAC MIIDR register value: Data read from the selected PHY register (correct read )
  */
uint16_t ETH_ReadPHYRegister(uint16_t PHYAddress, uint16_t PHYReg)
{
  uint32_t tmpreg = 0;     
__IO uint32_t timeout = 0;
  /* Check the parameters */
  assert_param(IS_ETH_PHY_ADDRESS(PHYAddress));
  assert_param(IS_ETH_PHY_REG(PHYReg));
  
  /* Get the ETHERNET MACMIIAR value */
  tmpreg = ETH->MACMIIAR;
  /* Keep only the CSR Clock Range CR[2:0] bits value */
  tmpreg &= ~MACMIIAR_CR_MASK;
  /* Prepare the MII address register value */
  tmpreg |=(((uint32_t)PHYAddress<<11) & ETH_MACMIIAR_PA); /* Set the PHY device address */
  tmpreg |=(((uint32_t)PHYReg<<6) & ETH_MACMIIAR_MR);      /* Set the PHY register address */
  tmpreg &= ~ETH_MACMIIAR_MW;                              /* Set the read mode */
  tmpreg |= ETH_MACMIIAR_MB;                               /* Set the MII Busy bit */
  /* Write the result value into the MII Address register */
  ETH->MACMIIAR = tmpreg;
  /* Check for the Busy flag */
  do
  {
    timeout++;
    tmpreg = ETH->MACMIIAR;
  } while ((tmpreg & ETH_MACMIIAR_MB) && (timeout < (uint32_t)PHY_READ_TO));
  /* Return ERROR in case of timeout */
  if(timeout == PHY_READ_TO)
  {
    return (uint16_t)ETH_ERROR;
  }
  
  /* Return data register value */
  return (uint16_t)(ETH->MACMIIDR);
}

/**
  * @brief  Write to a PHY register
  * @param PHYAddress: PHY device address, is the index of one of supported 32 PHY devices. 
  *   This parameter can be one of the following values: 0,..,31
  * @param PHYReg: PHY register address, is the index of one of the 32 PHY register. 
  *   This parameter can be one of the following values: 
  *     @arg PHY_BCR    : Tranceiver Control Register  
  *     @arg More PHY register could be written depending on the used PHY
  * @param  PHYValue: the value to write
  * @retval ETH_ERROR: in case of timeout
  *         ETH_SUCCESS: for correct write
  */
uint32_t ETH_WritePHYRegister(uint16_t PHYAddress, uint16_t PHYReg, uint16_t PHYValue)
{
  uint32_t tmpreg = 0;     
  __IO uint32_t timeout = 0;
  /* Check the parameters */
  assert_param(IS_ETH_PHY_ADDRESS(PHYAddress));
  assert_param(IS_ETH_PHY_REG(PHYReg));
  
  /* Get the ETHERNET MACMIIAR value */
  tmpreg = ETH->MACMIIAR;
  /* Keep only the CSR Clock Range CR[2:0] bits value */
  tmpreg &= ~MACMIIAR_CR_MASK;
  /* Prepare the MII register address value */
  tmpreg |=(((uint32_t)PHYAddress<<11) & ETH_MACMIIAR_PA); /* Set the PHY device address */
  tmpreg |=(((uint32_t)PHYReg<<6) & ETH_MACMIIAR_MR);      /* Set the PHY register address */
  tmpreg |= ETH_MACMIIAR_MW;                               /* Set the write mode */
  tmpreg |= ETH_MACMIIAR_MB;                               /* Set the MII Busy bit */
  /* Give the value to the MII data register */
  ETH->MACMIIDR = PHYValue;
  /* Write the result value into the MII Address register */
  ETH->MACMIIAR = tmpreg;
  /* Check for the Busy flag */
  do
  {
    timeout++;
    tmpreg = ETH->MACMIIAR;
  } while ((tmpreg & ETH_MACMIIAR_MB) && (timeout < (uint32_t)PHY_WRITE_TO));
  /* Return ERROR in case of timeout */
  if(timeout == PHY_WRITE_TO)
  {
    return ETH_ERROR;
  }
  
  /* Return SUCCESS */
  return ETH_SUCCESS;  
}

/**
  * @brief  Enables or disables the PHY loopBack mode.
  * @Note: Don't be confused with ETH_MACLoopBackCmd function which enables internal
  *  loopback at MII level
  * @param  PHYAddress: PHY device address, is the index of one of supported 32 PHY devices. 
  *   This parameter can be one of the following values:                   
  * @param  NewState: new state of the PHY loopBack mode.
  *   This parameter can be: ENABLE or DISABLE.    
  * @retval ETH_ERROR: in case of bad PHY configuration
  *         ETH_SUCCESS: for correct PHY configuration
  */
uint32_t ETH_PHYLoopBackCmd(uint16_t PHYAddress, FunctionalState NewState)
{
  uint16_t tmpreg = 0;
  /* Check the parameters */
  assert_param(IS_ETH_PHY_ADDRESS(PHYAddress));
  assert_param(IS_FUNCTIONAL_STATE(NewState));
    
  /* Get the PHY configuration to update it */
  tmpreg = ETH_ReadPHYRegister(PHYAddress, PHY_BCR); 
  
  if (NewState != DISABLE)
  {
    /* Enable the PHY loopback mode */
    tmpreg |= PHY_Loopback;  
  }
  else
  {
    /* Disable the PHY loopback mode: normal mode */
    tmpreg &= (uint16_t)(~(uint16_t)PHY_Loopback);
  }
  /* Update the PHY control register with the new configuration */
  if(ETH_WritePHYRegister(PHYAddress, PHY_BCR, tmpreg) != (uint32_t)RESET)
  {
    return ETH_SUCCESS;
  }
  else
  {
    /* Return SUCCESS */
    return ETH_ERROR; 
  }   
}

/*---------------------------------  MAC  ------------------------------------*/
/**
  * @brief  Enables or disables the MAC transmission.
  * @param  NewState: new state of the MAC transmission.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_MACTransmissionCmd(FunctionalState NewState)
{ 
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the MAC transmission */
    ETH->MACCR |= ETH_MACCR_TE;  
  }
  else
  {
    /* Disable the MAC transmission */
    ETH->MACCR &= ~ETH_MACCR_TE;
  }
}

/**
  * @brief  Enables or disables the MAC reception.
  * @param  NewState: new state of the MAC reception.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_MACReceptionCmd(FunctionalState NewState)
{ 
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the MAC reception */
    ETH->MACCR |= ETH_MACCR_RE;  
  }
  else
  {
    /* Disable the MAC reception */
    ETH->MACCR &= ~ETH_MACCR_RE;
  }
}

/**
  * @brief  Checks whether the ETHERNET flow control busy bit is set or not.
  * @param  None
  * @retval The new state of flow control busy status bit (SET or RESET).
  */
FlagStatus ETH_GetFlowControlBusyStatus(void)
{
  FlagStatus bitstatus = RESET;
  /* The Flow Control register should not be written to until this bit is cleared */
  if ((ETH->MACFCR & ETH_MACFCR_FCBBPA) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus;
}

/**
  * @brief  Initiate a Pause Control Frame (Full-duplex only).
  * @param  None
  * @retval None
  */
void ETH_InitiatePauseControlFrame(void)  
{ 
  /* When Set In full duplex MAC initiates pause control frame */
  ETH->MACFCR |= ETH_MACFCR_FCBBPA;  
}

/**
  * @brief  Enables or disables the MAC BackPressure operation activation (Half-duplex only).
  * @param  NewState: new state of the MAC BackPressure operation activation.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_BackPressureActivationCmd(FunctionalState NewState)   
{ 
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
    
  if (NewState != DISABLE)
  {
    /* Activate the MAC BackPressure operation */
    /* In Half duplex: during backpressure, when the MAC receives a new frame,
    the transmitter starts sending a JAM pattern resulting in a collision */
    ETH->MACFCR |= ETH_MACFCR_FCBBPA; 
  }
  else
  {
    /* Desactivate the MAC BackPressure operation */
    ETH->MACFCR &= ~ETH_MACFCR_FCBBPA; 
  } 
}

/**
  * @brief  Checks whether the specified ETHERNET MAC flag is set or not.
  * @param  ETH_MAC_FLAG: specifies the flag to check.
  *   This parameter can be one of the following values:
  *     @arg ETH_MAC_FLAG_TST  : Time stamp trigger flag   
  *     @arg ETH_MAC_FLAG_MMCT : MMC transmit flag  
  *     @arg ETH_MAC_FLAG_MMCR : MMC receive flag   
  *     @arg ETH_MAC_FLAG_MMC  : MMC flag  
  *     @arg ETH_MAC_FLAG_PMT  : PMT flag  
  * @retval The new state of ETHERNET MAC flag (SET or RESET).
  */
FlagStatus ETH_GetMACFlagStatus(uint32_t ETH_MAC_FLAG)
{
  FlagStatus bitstatus = RESET;
  /* Check the parameters */
  assert_param(IS_ETH_MAC_GET_FLAG(ETH_MAC_FLAG)); 
  if ((ETH->MACSR & ETH_MAC_FLAG) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus;
}

/**
  * @brief  Checks whether the specified ETHERNET MAC interrupt has occurred or not.
  * @param  ETH_MAC_IT: specifies the interrupt source to check.
  *   This parameter can be one of the following values:
  *     @arg ETH_MAC_IT_TST   : Time stamp trigger interrupt  
  *     @arg ETH_MAC_IT_MMCT : MMC transmit interrupt 
  *     @arg ETH_MAC_IT_MMCR : MMC receive interrupt  
  *     @arg ETH_MAC_IT_MMC  : MMC interrupt 
  *     @arg ETH_MAC_IT_PMT  : PMT interrupt 
  * @retval The new state of ETHERNET MAC interrupt (SET or RESET).
  */
ITStatus ETH_GetMACITStatus(uint32_t ETH_MAC_IT)
{
  ITStatus bitstatus = RESET;
  /* Check the parameters */
  assert_param(IS_ETH_MAC_GET_IT(ETH_MAC_IT)); 
  if ((ETH->MACSR & ETH_MAC_IT) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus;
}

/**
  * @brief  Enables or disables the specified ETHERNET MAC interrupts.
  * @param  ETH_MAC_IT: specifies the ETHERNET MAC interrupt sources to be
  *   enabled or disabled.
  *   This parameter can be any combination of the following values:
  *     @arg ETH_MAC_IT_TST : Time stamp trigger interrupt 
  *     @arg ETH_MAC_IT_PMT : PMT interrupt 
  * @param  NewState: new state of the specified ETHERNET MAC interrupts.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_MACITConfig(uint32_t ETH_MAC_IT, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_ETH_MAC_IT(ETH_MAC_IT));
  assert_param(IS_FUNCTIONAL_STATE(NewState));  
  
  if (NewState != DISABLE)
  {
    /* Enable the selected ETHERNET MAC interrupts */
    ETH->MACIMR &= (~(uint32_t)ETH_MAC_IT);
  }
  else
  {
    /* Disable the selected ETHERNET MAC interrupts */
    ETH->MACIMR |= ETH_MAC_IT;
  }
}

/**
  * @brief  Configures the selected MAC address.
  * @param  MacAddr: The MAC addres to configure.
  *   This parameter can be one of the following values:
  *     @arg ETH_MAC_Address0 : MAC Address0 
  *     @arg ETH_MAC_Address1 : MAC Address1 
  *     @arg ETH_MAC_Address2 : MAC Address2
  *     @arg ETH_MAC_Address3 : MAC Address3
  * @param  Addr: Pointer on MAC address buffer data (6 bytes).
  * @retval None
  */
void ETH_MACAddressConfig(uint32_t MacAddr, uint8_t *Addr)
{
  uint32_t tmpreg;
  /* Check the parameters */
  assert_param(IS_ETH_MAC_ADDRESS0123(MacAddr));
  
  /* Calculate the selectecd MAC address high register */
  tmpreg = ((uint32_t)Addr[5] << 8) | (uint32_t)Addr[4];
  /* Load the selectecd MAC address high register */
  (*(__IO uint32_t *) (ETH_MAC_ADDR_HBASE + MacAddr)) = tmpreg;
  /* Calculate the selectecd MAC address low register */
  tmpreg = ((uint32_t)Addr[3] << 24) | ((uint32_t)Addr[2] << 16) | ((uint32_t)Addr[1] << 8) | Addr[0];
 
  /* Load the selectecd MAC address low register */
  (*(__IO uint32_t *) (ETH_MAC_ADDR_LBASE + MacAddr)) = tmpreg;
}

/**
  * @brief  Get the selected MAC address.
  * @param  MacAddr: The MAC addres to return.
  *   This parameter can be one of the following values:
  *     @arg ETH_MAC_Address0 : MAC Address0 
  *     @arg ETH_MAC_Address1 : MAC Address1 
  *     @arg ETH_MAC_Address2 : MAC Address2
  *     @arg ETH_MAC_Address3 : MAC Address3
  * @param  Addr: Pointer on MAC address buffer data (6 bytes).
  * @retval None
  */
void ETH_GetMACAddress(uint32_t MacAddr, uint8_t *Addr)
{
  uint32_t tmpreg;
  /* Check the parameters */
  assert_param(IS_ETH_MAC_ADDRESS0123(MacAddr));
  
  /* Get the selectecd MAC address high register */
  tmpreg =(*(__IO uint32_t *) (ETH_MAC_ADDR_HBASE + MacAddr));
 
  /* Calculate the selectecd MAC address buffer */
  Addr[5] = ((tmpreg >> 8) & (uint8_t)0xFF);
  Addr[4] = (tmpreg & (uint8_t)0xFF);
  /* Load the selectecd MAC address low register */
  tmpreg =(*(__IO uint32_t *) (ETH_MAC_ADDR_LBASE + MacAddr));
  /* Calculate the selectecd MAC address buffer */
  Addr[3] = ((tmpreg >> 24) & (uint8_t)0xFF);
  Addr[2] = ((tmpreg >> 16) & (uint8_t)0xFF);
  Addr[1] = ((tmpreg >> 8 ) & (uint8_t)0xFF);
  Addr[0] = (tmpreg & (uint8_t)0xFF);
}

/**
  * @brief  Enables or disables the Address filter module uses the specified
  *   ETHERNET MAC address for perfect filtering 
  * @param  MacAddr: specifies the ETHERNET MAC address to be used for prfect filtering.
  *   This parameter can be one of the following values: 
  *     @arg ETH_MAC_Address1 : MAC Address1 
  *     @arg ETH_MAC_Address2 : MAC Address2
  *     @arg ETH_MAC_Address3 : MAC Address3
  * @param  NewState: new state of the specified ETHERNET MAC address use.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_MACAddressPerfectFilterCmd(uint32_t MacAddr, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_ETH_MAC_ADDRESS123(MacAddr));
  assert_param(IS_FUNCTIONAL_STATE(NewState));
    
  if (NewState != DISABLE)
  {
    /* Enable the selected ETHERNET MAC address for perfect filtering */
    (*(__IO uint32_t *) (ETH_MAC_ADDR_HBASE + MacAddr)) |= ETH_MACA1HR_AE;
  }
  else
  {
    /* Disable the selected ETHERNET MAC address for perfect filtering */
    (*(__IO uint32_t *) (ETH_MAC_ADDR_HBASE + MacAddr)) &=(~(uint32_t)ETH_MACA1HR_AE);
  }
}

/**
  * @brief  Set the filter type for the specified ETHERNET MAC address 
  * @param  MacAddr: specifies the ETHERNET MAC address 
  *   This parameter can be one of the following values: 
  *     @arg ETH_MAC_Address1 : MAC Address1 
  *     @arg ETH_MAC_Address2 : MAC Address2
  *     @arg ETH_MAC_Address3 : MAC Address3
  * @param  Filter: specifies the used frame received field for comparaison 
  *   This parameter can be one of the following values: 
  *     @arg ETH_MAC_AddressFilter_SA : MAC Address is used to compare with the
  *                                     SA fields of the received frame.
  *     @arg ETH_MAC_AddressFilter_DA : MAC Address is used to compare with the
  *                                     DA fields of the received frame.
  * @retval None
  */
void ETH_MACAddressFilterConfig(uint32_t MacAddr, uint32_t Filter)
{
  /* Check the parameters */
  assert_param(IS_ETH_MAC_ADDRESS123(MacAddr));
  assert_param(IS_ETH_MAC_ADDRESS_FILTER(Filter));
  
  if (Filter != ETH_MAC_AddressFilter_DA)
  {
    /* The selected ETHERNET MAC address is used to compare with the SA fields of the
       received frame. */
    (*(__IO uint32_t *) (ETH_MAC_ADDR_HBASE + MacAddr)) |= ETH_MACA1HR_SA;
  }
  else
  {
    /* The selected ETHERNET MAC address is used to compare with the DA fields of the
       received frame. */
    (*(__IO uint32_t *) (ETH_MAC_ADDR_HBASE + MacAddr)) &=(~(uint32_t)ETH_MACA1HR_SA);
  }
}

/**
  * @brief  Set the filter type for the specified ETHERNET MAC address 
  * @param  MacAddr: specifies the ETHERNET MAC address 
  *   This parameter can be one of the following values: 
  *     @arg ETH_MAC_Address1 : MAC Address1 
  *     @arg ETH_MAC_Address2 : MAC Address2
  *     @arg ETH_MAC_Address3 : MAC Address3
  * @param  MaskByte: specifies the used address bytes for comparaison 
  *   This parameter can be any combination of the following values: 
  *     @arg ETH_MAC_AddressMask_Byte6 : Mask MAC Address high reg bits [15:8].
  *     @arg ETH_MAC_AddressMask_Byte5 : Mask MAC Address high reg bits [7:0].
  *     @arg ETH_MAC_AddressMask_Byte4 : Mask MAC Address low reg bits [31:24].
  *     @arg ETH_MAC_AddressMask_Byte3 : Mask MAC Address low reg bits [23:16].
  *     @arg ETH_MAC_AddressMask_Byte2 : Mask MAC Address low reg bits [15:8].
  *     @arg ETH_MAC_AddressMask_Byte1 : Mask MAC Address low reg bits [7:0].
  * @retval None
  */
void ETH_MACAddressMaskBytesFilterConfig(uint32_t MacAddr, uint32_t MaskByte)
{
  /* Check the parameters */
  assert_param(IS_ETH_MAC_ADDRESS123(MacAddr));
  assert_param(IS_ETH_MAC_ADDRESS_MASK(MaskByte));
  
  /* Clear MBC bits in the selected MAC address  high register */
  (*(__IO uint32_t *) (ETH_MAC_ADDR_HBASE + MacAddr)) &=(~(uint32_t)ETH_MACA1HR_MBC);
  /* Set the selected Filetr mask bytes */
  (*(__IO uint32_t *) (ETH_MAC_ADDR_HBASE + MacAddr)) |= MaskByte;
}
/*------------------------  DMA Tx/Rx Desciptors -----------------------------*/

/**
  * @brief  Initializes the DMA Tx descriptors in chain mode.
  * @param  DMATxDescTab: Pointer on the first Tx desc list 
  * @param  TxBuff: Pointer on the first TxBuffer list
  * @param  TxBuffCount: Number of the used Tx desc in the list
  * @retval None
  */
void ETH_DMATxDescChainInit(ETH_DMADESCTypeDef *DMATxDescTab, uint8_t* TxBuff, uint32_t TxBuffCount)
{
  uint32_t i = 0;
  ETH_DMADESCTypeDef *DMATxDesc;
  
  /* Set the DMATxDescToSet pointer with the first one of the DMATxDescTab list */
  DMATxDescToSet = DMATxDescTab;
  /* Fill each DMATxDesc descriptor with the right values */   
  for(i=0; i < TxBuffCount; i++)
  {
    /* Get the pointer on the ith member of the Tx Desc list */
    DMATxDesc = DMATxDescTab + i;
    /* Set Second Address Chained bit */
    DMATxDesc->Status = ETH_DMATxDesc_TCH;  
       
    /* Set Buffer1 address pointer */
    DMATxDesc->Buffer1Addr = (uint32_t)(&TxBuff[i*ETH_MAX_PACKET_SIZE]);
    
    /* Initialize the next descriptor with the Next Desciptor Polling Enable */
    if(i < (TxBuffCount-1))
    {
      /* Set next descriptor address register with next descriptor base address */
      DMATxDesc->Buffer2NextDescAddr = (uint32_t)(DMATxDescTab+i+1);
    }
    else
    {
      /* For last descriptor, set next descriptor address register equal to the first descriptor base address */ 
      DMATxDesc->Buffer2NextDescAddr = (uint32_t) DMATxDescTab;  
    }
  }
   
  /* Set Transmit Desciptor List Address Register */
  ETH->DMATDLAR = (uint32_t) DMATxDescTab;
}

/**
  * @brief  Initializes the DMA Tx descriptors in ring mode.
  * @param  DMATxDescTab: Pointer on the first Tx desc list 
  * @param  TxBuff1: Pointer on the first TxBuffer1 list 
  * @param  TxBuff2: Pointer on the first TxBuffer2 list
  * @param  TxBuffCount: Number of the used Tx desc in the list
  *   Note: see decriptor skip length defined in ETH_DMA_InitStruct
  *   for the number of Words to skip between two unchained descriptors.  
  * @retval None
  */
void ETH_DMATxDescRingInit(ETH_DMADESCTypeDef *DMATxDescTab, uint8_t *TxBuff1, uint8_t *TxBuff2, uint32_t TxBuffCount)
{
  uint32_t i = 0;
  ETH_DMADESCTypeDef *DMATxDesc;
 
  /* Set the DMATxDescToSet pointer with the first one of the DMATxDescTab list */
  DMATxDescToSet = DMATxDescTab;
  /* Fill each DMATxDesc descriptor with the right values */  
  for(i=0; i < TxBuffCount; i++)
  {
    /* Get the pointer on the ith member of the Tx Desc list */
    DMATxDesc = DMATxDescTab + i;
    /* Set Buffer1 address pointer */
    DMATxDesc->Buffer1Addr = (uint32_t)(&TxBuff1[i*ETH_MAX_PACKET_SIZE]);
    
    /* Set Buffer2 address pointer */
    DMATxDesc->Buffer2NextDescAddr = (uint32_t)(&TxBuff2[i*ETH_MAX_PACKET_SIZE]);
    
    /* Set Transmit End of Ring bit for last descriptor: The DMA returns to the base
       address of the list, creating a Desciptor Ring */
    if(i == (TxBuffCount-1))
    {
      /* Set Transmit End of Ring bit */
      DMATxDesc->Status = ETH_DMATxDesc_TER;
    }
  }
   
  /* Set Transmit Desciptor List Address Register */
  ETH->DMATDLAR =  (uint32_t) DMATxDescTab;
}

/**
  * @brief  Checks whether the specified ETHERNET DMA Tx Desc flag is set or not.
  * @param  DMATxDesc: pointer on a DMA Tx descriptor
  * @param  ETH_DMATxDescFlag: specifies the flag to check.
  *   This parameter can be one of the following values:
  *     @arg ETH_DMATxDesc_OWN : OWN bit: descriptor is owned by DMA engine
  *     @arg ETH_DMATxDesc_IC  : Interrupt on completetion
  *     @arg ETH_DMATxDesc_LS  : Last Segment
  *     @arg ETH_DMATxDesc_FS  : First Segment
  *     @arg ETH_DMATxDesc_DC  : Disable CRC
  *     @arg ETH_DMATxDesc_DP  : Disable Pad
  *     @arg ETH_DMATxDesc_TTSE: Transmit Time Stamp Enable
  *     @arg ETH_DMATxDesc_TER : Transmit End of Ring
  *     @arg ETH_DMATxDesc_TCH : Second Address Chained
  *     @arg ETH_DMATxDesc_TTSS: Tx Time Stamp Status
  *     @arg ETH_DMATxDesc_IHE : IP Header Error
  *     @arg ETH_DMATxDesc_ES  : Error summary
  *     @arg ETH_DMATxDesc_JT  : Jabber Timeout
  *     @arg ETH_DMATxDesc_FF  : Frame Flushed: DMA/MTL flushed the frame due to SW flush
  *     @arg ETH_DMATxDesc_PCE : Payload Checksum Error
  *     @arg ETH_DMATxDesc_LCA : Loss of Carrier: carrier lost during tramsmission
  *     @arg ETH_DMATxDesc_NC  : No Carrier: no carrier signal from the tranceiver
  *     @arg ETH_DMATxDesc_LCO : Late Collision: transmission aborted due to collision
  *     @arg ETH_DMATxDesc_EC  : Excessive Collision: transmission aborted after 16 collisions
  *     @arg ETH_DMATxDesc_VF  : VLAN Frame
  *     @arg ETH_DMATxDesc_CC  : Collision Count 
  *     @arg ETH_DMATxDesc_ED  : Excessive Deferral
  *     @arg ETH_DMATxDesc_UF  : Underflow Error: late data arrival from the memory
  *     @arg ETH_DMATxDesc_DB  : Deferred Bit
  * @retval The new state of ETH_DMATxDescFlag (SET or RESET).
  */
FlagStatus ETH_GetDMATxDescFlagStatus(ETH_DMADESCTypeDef *DMATxDesc, uint32_t ETH_DMATxDescFlag)
{
  FlagStatus bitstatus = RESET;
  /* Check the parameters */
  assert_param(IS_ETH_DMATxDESC_GET_FLAG(ETH_DMATxDescFlag));
  
  if ((DMATxDesc->Status & ETH_DMATxDescFlag) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus;
}

/**
  * @brief  Returns the specified ETHERNET DMA Tx Desc collision count.
  * @param  DMATxDesc: pointer on a DMA Tx descriptor                     
  * @retval The Transmit descriptor collision counter value.
  */
uint32_t ETH_GetDMATxDescCollisionCount(ETH_DMADESCTypeDef *DMATxDesc)
{
  /* Return the Receive descriptor frame length */
  return ((DMATxDesc->Status & ETH_DMATxDesc_CC) >> ETH_DMATXDESC_COLLISION_COUNTSHIFT);
}

/**
  * @brief  Set the specified DMA Tx Desc Own bit.
  * @param  DMATxDesc: Pointer on a Tx desc
  * @retval None
  */
void ETH_SetDMATxDescOwnBit(ETH_DMADESCTypeDef *DMATxDesc)
{
  /* Set the DMA Tx Desc Own bit */
  DMATxDesc->Status |= ETH_DMATxDesc_OWN;
}

/**
  * @brief  Enables or disables the specified DMA Tx Desc Transmit interrupt.
  * @param  DMATxDesc: Pointer on a Tx desc
  * @param  NewState: new state of the DMA Tx Desc transmit interrupt.
  *   This parameter can be: ENABLE or DISABLE.                   
  * @retval None
  */
void ETH_DMATxDescTransmitITConfig(ETH_DMADESCTypeDef *DMATxDesc, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the DMA Tx Desc Transmit interrupt */
    DMATxDesc->Status |= ETH_DMATxDesc_IC;
  }
  else
  {
    /* Disable the DMA Tx Desc Transmit interrupt */
    DMATxDesc->Status &=(~(uint32_t)ETH_DMATxDesc_IC);
  }
}

/**
  * @brief  Enables or disables the specified DMA Tx Desc Transmit interrupt.
  * @param  DMATxDesc: Pointer on a Tx desc
  * @param  DMATxDesc_FrameSegment: specifies is the actual Tx desc contain last or first segment.
  *   This parameter can be one of the following values:
  *     @arg ETH_DMATxDesc_LastSegment  : actual Tx desc contain last segment 
  *     @arg ETH_DMATxDesc_FirstSegment : actual Tx desc contain first segment                   
  * @retval None
  */
void ETH_DMATxDescFrameSegmentConfig(ETH_DMADESCTypeDef *DMATxDesc, uint32_t DMATxDesc_FrameSegment)
{
  /* Check the parameters */
  assert_param(IS_ETH_DMA_TXDESC_SEGMENT(DMATxDesc_FrameSegment));
  
  /* Selects the DMA Tx Desc Frame segment */
  DMATxDesc->Status |= DMATxDesc_FrameSegment;
}

/**
  * @brief  Selects the specified ETHERNET DMA Tx Desc Checksum Insertion.
  * @param  DMATxDesc: pointer on a DMA Tx descriptor 
  * @param  DMATxDesc_Checksum: specifies is the DMA Tx desc checksum insertion.
  *   This parameter can be one of the following values:
  *     @arg ETH_DMATxDesc_ChecksumByPass : Checksum bypass
  *     @arg ETH_DMATxDesc_ChecksumIPV4Header : IPv4 header checksum
  *     @arg ETH_DMATxDesc_ChecksumTCPUDPICMPSegment : TCP/UDP/ICMP checksum. Pseudo header checksum is assumed to be present
  *     @arg ETH_DMATxDesc_ChecksumTCPUDPICMPFull : TCP/UDP/ICMP checksum fully in hardware including pseudo header                                                                
  * @retval None
  */
void ETH_DMATxDescChecksumInsertionConfig(ETH_DMADESCTypeDef *DMATxDesc, uint32_t DMATxDesc_Checksum)
{
  /* Check the parameters */
  assert_param(IS_ETH_DMA_TXDESC_CHECKSUM(DMATxDesc_Checksum));
  
  /* Set the selected DMA Tx desc checksum insertion control */
  DMATxDesc->Status |= DMATxDesc_Checksum;
}

/**
  * @brief  Enables or disables the DMA Tx Desc CRC.
  * @param  DMATxDesc: pointer on a DMA Tx descriptor
  * @param  NewState: new state of the specified DMA Tx Desc CRC.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_DMATxDescCRCCmd(ETH_DMADESCTypeDef *DMATxDesc, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the selected DMA Tx Desc CRC */
    DMATxDesc->Status &= (~(uint32_t)ETH_DMATxDesc_DC);
  }
  else
  {
    /* Disable the selected DMA Tx Desc CRC */
    DMATxDesc->Status |= ETH_DMATxDesc_DC; 
  }
}

/**
  * @brief  Enables or disables the DMA Tx Desc end of ring.
  * @param  DMATxDesc: pointer on a DMA Tx descriptor
  * @param  NewState: new state of the specified DMA Tx Desc end of ring.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_DMATxDescEndOfRingCmd(ETH_DMADESCTypeDef *DMATxDesc, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the selected DMA Tx Desc end of ring */
    DMATxDesc->Status |= ETH_DMATxDesc_TER;  
  }
  else
  {
    /* Disable the selected DMA Tx Desc end of ring */
    DMATxDesc->Status &= (~(uint32_t)ETH_DMATxDesc_TER); 
  }
}

/**
  * @brief  Enables or disables the DMA Tx Desc second address chained.
  * @param  DMATxDesc: pointer on a DMA Tx descriptor
  * @param  NewState: new state of the specified DMA Tx Desc second address chained.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_DMATxDescSecondAddressChainedCmd(ETH_DMADESCTypeDef *DMATxDesc, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the selected DMA Tx Desc second address chained */
    DMATxDesc->Status |= ETH_DMATxDesc_TCH;  
  }
  else
  {
    /* Disable the selected DMA Tx Desc second address chained */
    DMATxDesc->Status &=(~(uint32_t)ETH_DMATxDesc_TCH); 
  }
}

/**
  * @brief  Enables or disables the DMA Tx Desc padding for frame shorter than 64 bytes.
  * @param  DMATxDesc: pointer on a DMA Tx descriptor
  * @param  NewState: new state of the specified DMA Tx Desc padding for frame shorter than 64 bytes.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_DMATxDescShortFramePaddingCmd(ETH_DMADESCTypeDef *DMATxDesc, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the selected DMA Tx Desc padding for frame shorter than 64 bytes */
    DMATxDesc->Status &= (~(uint32_t)ETH_DMATxDesc_DP);
  }
  else
  {
    /* Disable the selected DMA Tx Desc padding for frame shorter than 64 bytes*/
    DMATxDesc->Status |= ETH_DMATxDesc_DP; 
  }
}

/**
  * @brief  Enables or disables the DMA Tx Desc time stamp.
  * @param  DMATxDesc: pointer on a DMA Tx descriptor
  * @param  NewState: new state of the specified DMA Tx Desc time stamp.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_DMATxDescTimeStampCmd(ETH_DMADESCTypeDef *DMATxDesc, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the selected DMA Tx Desc time stamp */
    DMATxDesc->Status |= ETH_DMATxDesc_TTSE;  
  }
  else
  {
    /* Disable the selected DMA Tx Desc time stamp */
    DMATxDesc->Status &=(~(uint32_t)ETH_DMATxDesc_TTSE); 
  }
}

/**
  * @brief  Configures the specified DMA Tx Desc buffer1 and buffer2 sizes.
  * @param  DMATxDesc: Pointer on a Tx desc
  * @param  BufferSize1: specifies the Tx desc buffer1 size.
  * @param  BufferSize2: specifies the Tx desc buffer2 size (put "0" if not used).
  * @retval None
  */
void ETH_DMATxDescBufferSizeConfig(ETH_DMADESCTypeDef *DMATxDesc, uint32_t BufferSize1, uint32_t BufferSize2)
{
  /* Check the parameters */
  assert_param(IS_ETH_DMATxDESC_BUFFER_SIZE(BufferSize1));
  assert_param(IS_ETH_DMATxDESC_BUFFER_SIZE(BufferSize2));
  
  /* Set the DMA Tx Desc buffer1 and buffer2 sizes values */
  DMATxDesc->ControlBufferSize |= (BufferSize1 | (BufferSize2 << ETH_DMATXDESC_BUFFER2_SIZESHIFT));
}

/**
  * @brief  Initializes the DMA Rx descriptors in chain mode.
  * @param  DMARxDescTab: Pointer on the first Rx desc list 
  * @param  RxBuff: Pointer on the first RxBuffer list
  * @param  RxBuffCount: Number of the used Rx desc in the list
  * @retval None
  */
void ETH_DMARxDescChainInit(ETH_DMADESCTypeDef *DMARxDescTab, uint8_t *RxBuff, uint32_t RxBuffCount)
{
  uint32_t i = 0;
  ETH_DMADESCTypeDef *DMARxDesc;
  
  /* Set the DMARxDescToGet pointer with the first one of the DMARxDescTab list */
  DMARxDescToGet = DMARxDescTab; 
  /* Fill each DMARxDesc descriptor with the right values */
  for(i=0; i < RxBuffCount; i++)
  {
    /* Get the pointer on the ith member of the Rx Desc list */
    DMARxDesc = DMARxDescTab+i;
    /* Set Own bit of the Rx descriptor Status */
    DMARxDesc->Status = ETH_DMARxDesc_OWN;

    /* Set Buffer1 size and Second Address Chained bit */
    DMARxDesc->ControlBufferSize = ETH_DMARxDesc_RCH | (uint32_t)ETH_MAX_PACKET_SIZE;  
    /* Set Buffer1 address pointer */
    DMARxDesc->Buffer1Addr = (uint32_t)(&RxBuff[i*ETH_MAX_PACKET_SIZE]);
    
    /* Initialize the next descriptor with the Next Desciptor Polling Enable */
    if(i < (RxBuffCount-1))
    {
      /* Set next descriptor address register with next descriptor base address */
      DMARxDesc->Buffer2NextDescAddr = (uint32_t)(DMARxDescTab+i+1); 
    }
    else
    {
      /* For last descriptor, set next descriptor address register equal to the first descriptor base address */ 
      DMARxDesc->Buffer2NextDescAddr = (uint32_t)(DMARxDescTab); 
    }
  }
   
  /* Set Receive Desciptor List Address Register */
  ETH->DMARDLAR = (uint32_t) DMARxDescTab;  
}

/**
  * @brief  Initializes the DMA Rx descriptors in ring mode.
  * @param  DMARxDescTab: Pointer on the first Rx desc list 
  * @param  RxBuff1: Pointer on the first RxBuffer1 list 
  * @param  RxBuff2: Pointer on the first RxBuffer2 list
  * @param  RxBuffCount: Number of the used Rx desc in the list
  *   Note: see decriptor skip length defined in ETH_DMA_InitStruct
  *   for the number of Words to skip between two unchained descriptors.  
  * @retval None
  */
void ETH_DMARxDescRingInit(ETH_DMADESCTypeDef *DMARxDescTab, uint8_t *RxBuff1, uint8_t *RxBuff2, uint32_t RxBuffCount)
{
  uint32_t i = 0;
  ETH_DMADESCTypeDef *DMARxDesc;
  /* Set the DMARxDescToGet pointer with the first one of the DMARxDescTab list */
  DMARxDescToGet = DMARxDescTab; 
  /* Fill each DMARxDesc descriptor with the right values */  
  for(i=0; i < RxBuffCount; i++)
  {
    /* Get the pointer on the ith member of the Rx Desc list */
    DMARxDesc = DMARxDescTab+i; 
    /* Set Own bit of the Rx descriptor Status */
    DMARxDesc->Status = ETH_DMARxDesc_OWN; 
    /* Set Buffer1 size */
    DMARxDesc->ControlBufferSize = ETH_MAX_PACKET_SIZE;  
    /* Set Buffer1 address pointer */
    DMARxDesc->Buffer1Addr = (uint32_t)(&RxBuff1[i*ETH_MAX_PACKET_SIZE]); 
    
    /* Set Buffer2 address pointer */
    DMARxDesc->Buffer2NextDescAddr = (uint32_t)(&RxBuff2[i*ETH_MAX_PACKET_SIZE]); 
    
    /* Set Receive End of Ring bit for last descriptor: The DMA returns to the base
       address of the list, creating a Desciptor Ring */
    if(i == (RxBuffCount-1))
    {
      /* Set Receive End of Ring bit */
      DMARxDesc->ControlBufferSize |= ETH_DMARxDesc_RER;
    }
  }
   
  /* Set Receive Desciptor List Address Register */
  ETH->DMARDLAR = (uint32_t) DMARxDescTab;  
}

/**
  * @brief  Checks whether the specified ETHERNET Rx Desc flag is set or not.
  * @param  DMARxDesc: pointer on a DMA Rx descriptor
  * @param  ETH_DMARxDescFlag: specifies the flag to check.
  *   This parameter can be one of the following values:
  *     @arg ETH_DMARxDesc_OWN:         OWN bit: descriptor is owned by DMA engine 
  *     @arg ETH_DMARxDesc_AFM:         DA Filter Fail for the rx frame
  *     @arg ETH_DMARxDesc_ES:          Error summary
  *     @arg ETH_DMARxDesc_DE:          Desciptor error: no more descriptors for receive frame
  *     @arg ETH_DMARxDesc_SAF:         SA Filter Fail for the received frame
  *     @arg ETH_DMARxDesc_LE:          Frame size not matching with length field
  *     @arg ETH_DMARxDesc_OE:          Overflow Error: Frame was damaged due to buffer overflow
  *     @arg ETH_DMARxDesc_VLAN:        VLAN Tag: received frame is a VLAN frame
  *     @arg ETH_DMARxDesc_FS:          First descriptor of the frame
  *     @arg ETH_DMARxDesc_LS:          Last descriptor of the frame
  *     @arg ETH_DMARxDesc_IPV4HCE:     IPC Checksum Error/Giant Frame: Rx Ipv4 header checksum error 
  *     @arg ETH_DMARxDesc_LC:          Late collision occurred during reception
  *     @arg ETH_DMARxDesc_FT:          Frame type - Ethernet, otherwise 802.3
  *     @arg ETH_DMARxDesc_RWT:         Receive Watchdog Timeout: watchdog timer expired during reception
  *     @arg ETH_DMARxDesc_RE:          Receive error: error reported by MII interface
  *     @arg ETH_DMARxDesc_DE:          Dribble bit error: frame contains non int multiple of 8 bits
  *     @arg ETH_DMARxDesc_CE:          CRC error
  *     @arg ETH_DMARxDesc_MAMPCE:      Rx MAC Address/Payload Checksum Error: Rx MAC address matched/ Rx Payload Checksum Error
  * @retval The new state of ETH_DMARxDescFlag (SET or RESET).
  */
FlagStatus ETH_GetDMARxDescFlagStatus(ETH_DMADESCTypeDef *DMARxDesc, uint32_t ETH_DMARxDescFlag)
{
  FlagStatus bitstatus = RESET;
  /* Check the parameters */
  assert_param(IS_ETH_DMARxDESC_GET_FLAG(ETH_DMARxDescFlag));
  if ((DMARxDesc->Status & ETH_DMARxDescFlag) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus;
}

/**
  * @brief  Set the specified DMA Rx Desc Own bit.
  * @param  DMARxDesc: Pointer on a Rx desc
  * @retval None
  */
void ETH_SetDMARxDescOwnBit(ETH_DMADESCTypeDef *DMARxDesc)
{
  /* Set the DMA Rx Desc Own bit */
  DMARxDesc->Status |= ETH_DMARxDesc_OWN;
}

/**
  * @brief  Returns the specified DMA Rx Desc frame length.
  * @param  DMARxDesc: pointer on a DMA Rx descriptor                     
  * @retval The Rx descriptor received frame length.
  */
uint32_t ETH_GetDMARxDescFrameLength(ETH_DMADESCTypeDef *DMARxDesc)
{
  /* Return the Receive descriptor frame length */
  return ((DMARxDesc->Status & ETH_DMARxDesc_FL) >> ETH_DMARXDESC_FRAME_LENGTHSHIFT);
}

/**
  * @brief  Enables or disables the specified DMA Rx Desc receive interrupt.
  * @param  DMARxDesc: Pointer on a Rx desc
  * @param  NewState: new state of the specified DMA Rx Desc interrupt.
  *   This parameter can be: ENABLE or DISABLE.                   
  * @retval None
  */
void ETH_DMARxDescReceiveITConfig(ETH_DMADESCTypeDef *DMARxDesc, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the DMA Rx Desc receive interrupt */
    DMARxDesc->ControlBufferSize &=(~(uint32_t)ETH_DMARxDesc_DIC);
  }
  else
  {
    /* Disable the DMA Rx Desc receive interrupt */
    DMARxDesc->ControlBufferSize |= ETH_DMARxDesc_DIC;
  }
}

/**
  * @brief  Enables or disables the DMA Rx Desc end of ring.
  * @param  DMARxDesc: pointer on a DMA Rx descriptor
  * @param  NewState: new state of the specified DMA Rx Desc end of ring.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_DMARxDescEndOfRingCmd(ETH_DMADESCTypeDef *DMARxDesc, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the selected DMA Rx Desc end of ring */
    DMARxDesc->ControlBufferSize |= ETH_DMARxDesc_RER;  
  }
  else
  {
    /* Disable the selected DMA Rx Desc end of ring */
    DMARxDesc->ControlBufferSize &=(~(uint32_t)ETH_DMARxDesc_RER); 
  }
}

/**
  * @brief  Enables or disables the DMA Rx Desc second address chained.
  * @param  DMARxDesc: pointer on a DMA Rx descriptor
  * @param  NewState: new state of the specified DMA Rx Desc second address chained.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_DMARxDescSecondAddressChainedCmd(ETH_DMADESCTypeDef *DMARxDesc, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the selected DMA Rx Desc second address chained */
    DMARxDesc->ControlBufferSize |= ETH_DMARxDesc_RCH;  
  }
  else
  {
    /* Disable the selected DMA Rx Desc second address chained */
    DMARxDesc->ControlBufferSize &=(~(uint32_t)ETH_DMARxDesc_RCH); 
  }
}

/**
  * @brief  Returns the specified ETHERNET DMA Rx Desc buffer size.
  * @param  DMARxDesc: pointer on a DMA Rx descriptor 
  * @param  DMARxDesc_Buffer: specifies the DMA Rx Desc buffer.
  *   This parameter can be any one of the following values:
  *     @arg ETH_DMARxDesc_Buffer1 : DMA Rx Desc Buffer1
  *     @arg ETH_DMARxDesc_Buffer2 : DMA Rx Desc Buffer2                     
  * @retval The Receive descriptor frame length.
  */
uint32_t ETH_GetDMARxDescBufferSize(ETH_DMADESCTypeDef *DMARxDesc, uint32_t DMARxDesc_Buffer)
{
  /* Check the parameters */
  assert_param(IS_ETH_DMA_RXDESC_BUFFER(DMARxDesc_Buffer));
  
  if(DMARxDesc_Buffer != ETH_DMARxDesc_Buffer1)
  {
    /* Return the DMA Rx Desc buffer2 size */
    return ((DMARxDesc->ControlBufferSize & ETH_DMARxDesc_RBS2) >> ETH_DMARXDESC_BUFFER2_SIZESHIFT); 
  }
  else
  {
    /* Return the DMA Rx Desc buffer1 size */
    return (DMARxDesc->ControlBufferSize & ETH_DMARxDesc_RBS1); 
  }
}

/*---------------------------------  DMA  ------------------------------------*/
/**
  * @brief  Resets all MAC subsystem internal registers and logic.
  * @param  None
  * @retval None
  */
void ETH_SoftwareReset(void)
{
  /* Set the SWR bit: resets all MAC subsystem internal registers and logic */
  /* After reset all the registers holds their respective reset values */
  ETH->DMABMR |= ETH_DMABMR_SR;
}

/**
  * @brief  Checks whether the ETHERNET software reset bit is set or not.
  * @param  None
  * @retval The new state of DMA Bus Mode register SR bit (SET or RESET).
  */
FlagStatus ETH_GetSoftwareResetStatus(void)
{
  FlagStatus bitstatus = RESET;
  if((ETH->DMABMR & ETH_DMABMR_SR) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus;
}

/**
  * @brief  Checks whether the specified ETHERNET DMA flag is set or not.
  * @param  ETH_DMA_FLAG: specifies the flag to check.
  *   This parameter can be one of the following values:
  *     @arg ETH_DMA_FLAG_TST : Time-stamp trigger flag
  *     @arg ETH_DMA_FLAG_PMT : PMT flag 
  *     @arg ETH_DMA_FLAG_MMC : MMC flag 
  *     @arg ETH_DMA_FLAG_DataTransferError : Error bits 0-data buffer, 1-desc. access
  *     @arg ETH_DMA_FLAG_ReadWriteError    : Error bits 0-write trnsf, 1-read transfr
  *     @arg ETH_DMA_FLAG_AccessError       : Error bits 0-Rx DMA, 1-Tx DMA
  *     @arg ETH_DMA_FLAG_NIS : Normal interrupt summary flag
  *     @arg ETH_DMA_FLAG_AIS : Abnormal interrupt summary flag  
  *     @arg ETH_DMA_FLAG_ER  : Early receive flag 
  *     @arg ETH_DMA_FLAG_FBE : Fatal bus error flag 
  *     @arg ETH_DMA_FLAG_ET  : Early transmit flag 
  *     @arg ETH_DMA_FLAG_RWT : Receive watchdog timeout flag 
  *     @arg ETH_DMA_FLAG_RPS : Receive process stopped flag 
  *     @arg ETH_DMA_FLAG_RBU : Receive buffer unavailable flag 
  *     @arg ETH_DMA_FLAG_R   : Receive flag 
  *     @arg ETH_DMA_FLAG_TU  : Underflow flag 
  *     @arg ETH_DMA_FLAG_RO  : Overflow flag 
  *     @arg ETH_DMA_FLAG_TJT : Transmit jabber timeout flag 
  *     @arg ETH_DMA_FLAG_TBU : Transmit buffer unavailable flag 
  *     @arg ETH_DMA_FLAG_TPS : Transmit process stopped flag 
  *     @arg ETH_DMA_FLAG_T   : Transmit flag 
  * @retval The new state of ETH_DMA_FLAG (SET or RESET).
  */
FlagStatus ETH_GetDMAFlagStatus(uint32_t ETH_DMA_FLAG)
{  
  FlagStatus bitstatus = RESET;
  /* Check the parameters */
  assert_param(IS_ETH_DMA_GET_IT(ETH_DMA_FLAG));
  if ((ETH->DMASR & ETH_DMA_FLAG) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus;
}

/**
  * @brief  Clears the ETHERNETís DMA pending flag.
  * @param  ETH_DMA_FLAG: specifies the flag to clear.
  *   This parameter can be any combination of the following values:
  *     @arg ETH_DMA_FLAG_NIS : Normal interrupt summary flag
  *     @arg ETH_DMA_FLAG_AIS : Abnormal interrupt summary flag 
  *     @arg ETH_DMA_FLAG_ER  : Early receive flag 
  *     @arg ETH_DMA_FLAG_FBE : Fatal bus error flag 
  *     @arg ETH_DMA_FLAG_ETI : Early transmit flag 
  *     @arg ETH_DMA_FLAG_RWT : Receive watchdog timeout flag 
  *     @arg ETH_DMA_FLAG_RPS : Receive process stopped flag 
  *     @arg ETH_DMA_FLAG_RBU : Receive buffer unavailable flag 
  *     @arg ETH_DMA_FLAG_R   : Receive flag 
  *     @arg ETH_DMA_FLAG_TU  : Transmit Underflow flag 
  *     @arg ETH_DMA_FLAG_RO  : Receive Overflow flag 
  *     @arg ETH_DMA_FLAG_TJT : Transmit jabber timeout flag 
  *     @arg ETH_DMA_FLAG_TBU : Transmit buffer unavailable flag 
  *     @arg ETH_DMA_FLAG_TPS : Transmit process stopped flag 
  *     @arg ETH_DMA_FLAG_T   : Transmit flag
  * @retval None
  */
void ETH_DMAClearFlag(uint32_t ETH_DMA_FLAG)
{
  /* Check the parameters */
  assert_param(IS_ETH_DMA_FLAG(ETH_DMA_FLAG));
  
  /* Clear the selected ETHERNET DMA FLAG */
  ETH->DMASR = (uint32_t) ETH_DMA_FLAG;
}

/**
  * @brief  Checks whether the specified ETHERNET DMA interrupt has occured or not.
  * @param  ETH_DMA_IT: specifies the interrupt source to check.
  *   This parameter can be one of the following values:
  *     @arg ETH_DMA_IT_TST : Time-stamp trigger interrupt
  *     @arg ETH_DMA_IT_PMT : PMT interrupt 
  *     @arg ETH_DMA_IT_MMC : MMC interrupt
  *     @arg ETH_DMA_IT_NIS : Normal interrupt summary 
  *     @arg ETH_DMA_IT_AIS : Abnormal interrupt summary  
  *     @arg ETH_DMA_IT_ER  : Early receive interrupt 
  *     @arg ETH_DMA_IT_FBE : Fatal bus error interrupt 
  *     @arg ETH_DMA_IT_ET  : Early transmit interrupt 
  *     @arg ETH_DMA_IT_RWT : Receive watchdog timeout interrupt 
  *     @arg ETH_DMA_IT_RPS : Receive process stopped interrupt 
  *     @arg ETH_DMA_IT_RBU : Receive buffer unavailable interrupt 
  *     @arg ETH_DMA_IT_R   : Receive interrupt 
  *     @arg ETH_DMA_IT_TU  : Underflow interrupt 
  *     @arg ETH_DMA_IT_RO  : Overflow interrupt 
  *     @arg ETH_DMA_IT_TJT : Transmit jabber timeout interrupt 
  *     @arg ETH_DMA_IT_TBU : Transmit buffer unavailable interrupt 
  *     @arg ETH_DMA_IT_TPS : Transmit process stopped interrupt 
  *     @arg ETH_DMA_IT_T   : Transmit interrupt 
  * @retval The new state of ETH_DMA_IT (SET or RESET).
  */
ITStatus ETH_GetDMAITStatus(uint32_t ETH_DMA_IT)
{  
  ITStatus bitstatus = RESET;
  /* Check the parameters */
  assert_param(IS_ETH_DMA_GET_IT(ETH_DMA_IT));
  if ((ETH->DMASR & ETH_DMA_IT) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus;
}

/**
  * @brief  Clears the ETHERNETís DMA IT pending bit.
  * @param  ETH_DMA_IT: specifies the interrupt pending bit to clear.
  *   This parameter can be any combination of the following values:
  *     @arg ETH_DMA_IT_NIS : Normal interrupt summary 
  *     @arg ETH_DMA_IT_AIS : Abnormal interrupt summary  
  *     @arg ETH_DMA_IT_ER  : Early receive interrupt 
  *     @arg ETH_DMA_IT_FBE : Fatal bus error interrupt 
  *     @arg ETH_DMA_IT_ETI : Early transmit interrupt 
  *     @arg ETH_DMA_IT_RWT : Receive watchdog timeout interrupt 
  *     @arg ETH_DMA_IT_RPS : Receive process stopped interrupt 
  *     @arg ETH_DMA_IT_RBU : Receive buffer unavailable interrupt 
  *     @arg ETH_DMA_IT_R   : Receive interrupt 
  *     @arg ETH_DMA_IT_TU  : Transmit Underflow interrupt 
  *     @arg ETH_DMA_IT_RO  : Receive Overflow interrupt 
  *     @arg ETH_DMA_IT_TJT : Transmit jabber timeout interrupt 
  *     @arg ETH_DMA_IT_TBU : Transmit buffer unavailable interrupt 
  *     @arg ETH_DMA_IT_TPS : Transmit process stopped interrupt 
  *     @arg ETH_DMA_IT_T   : Transmit interrupt
  * @retval None
  */
void ETH_DMAClearITPendingBit(uint32_t ETH_DMA_IT)
{
  /* Check the parameters */
  assert_param(IS_ETH_DMA_IT(ETH_DMA_IT));
  
  /* Clear the selected ETHERNET DMA IT */
  ETH->DMASR = (uint32_t) ETH_DMA_IT;
}

/**
  * @brief  Returns the ETHERNET DMA Transmit Process State.
  * @param  None
  * @retval The new ETHERNET DMA Transmit Process State:
  *   This can be one of the following values:
  *     - ETH_DMA_TransmitProcess_Stopped   : Stopped - Reset or Stop Tx Command issued
  *     - ETH_DMA_TransmitProcess_Fetching  : Running - fetching the Tx descriptor 
  *     - ETH_DMA_TransmitProcess_Waiting   : Running - waiting for status
  *     - ETH_DMA_TransmitProcess_Reading   : unning - reading the data from host memory
  *     - ETH_DMA_TransmitProcess_Suspended : Suspended - Tx Desciptor unavailabe
  *     - ETH_DMA_TransmitProcess_Closing   : Running - closing Rx descriptor  
  */
uint32_t ETH_GetTransmitProcessState(void)
{
  return ((uint32_t)(ETH->DMASR & ETH_DMASR_TS)); 
}

/**
  * @brief  Returns the ETHERNET DMA Receive Process State.
  * @param  None
  * @retval The new ETHERNET DMA Receive Process State:
  *   This can be one of the following values:
  *     - ETH_DMA_ReceiveProcess_Stopped   : Stopped - Reset or Stop Rx Command issued
  *     - ETH_DMA_ReceiveProcess_Fetching  : Running - fetching the Rx descriptor 
  *     - ETH_DMA_ReceiveProcess_Waiting   : Running - waiting for packet
  *     - ETH_DMA_ReceiveProcess_Suspended : Suspended - Rx Desciptor unavailable
  *     - ETH_DMA_ReceiveProcess_Closing   : Running - closing descriptor
  *     - ETH_DMA_ReceiveProcess_Queuing   : Running - queuing the recieve frame into host memory  
  */
uint32_t ETH_GetReceiveProcessState(void)
{
  return ((uint32_t)(ETH->DMASR & ETH_DMASR_RS)); 
}

/**
  * @brief  Clears the ETHERNET transmit FIFO.
  * @param  None                
  * @retval None
  */
void ETH_FlushTransmitFIFO(void)
{
  /* Set the Flush Transmit FIFO bit */
  ETH->DMAOMR |= ETH_DMAOMR_FTF;  
}

/**
  * @brief  Checks whether the ETHERNET transmit FIFO bit is cleared or not.
  * @param  None                
  * @retval The new state of ETHERNET flush transmit FIFO bit (SET or RESET).
  */
FlagStatus ETH_GetFlushTransmitFIFOStatus(void)
{   
  FlagStatus bitstatus = RESET;
  if ((ETH->DMAOMR & ETH_DMAOMR_FTF) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus; 
}

/**
  * @brief  Enables or disables the DMA transmission.
  * @param  NewState: new state of the DMA transmission.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_DMATransmissionCmd(FunctionalState NewState)
{ 
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the DMA transmission */
    ETH->DMAOMR |= ETH_DMAOMR_ST;  
  }
  else
  {
    /* Disable the DMA transmission */
    ETH->DMAOMR &= ~ETH_DMAOMR_ST;
  }
}

/**
  * @brief  Enables or disables the DMA reception.
  * @param  NewState: new state of the DMA reception.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_DMAReceptionCmd(FunctionalState NewState)
{ 
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the DMA reception */
    ETH->DMAOMR |= ETH_DMAOMR_SR;  
  }
  else
  {
    /* Disable the DMA reception */
    ETH->DMAOMR &= ~ETH_DMAOMR_SR;
  }
}

/**
  * @brief  Enables or disables the specified ETHERNET DMA interrupts.
  * @param  ETH_DMA_IT: specifies the ETHERNET DMA interrupt sources to be
  *   enabled or disabled.
  *   This parameter can be any combination of the following values:
  *     @arg ETH_DMA_IT_NIS : Normal interrupt summary 
  *     @arg ETH_DMA_IT_AIS : Abnormal interrupt summary  
  *     @arg ETH_DMA_IT_ER  : Early receive interrupt 
  *     @arg ETH_DMA_IT_FBE : Fatal bus error interrupt 
  *     @arg ETH_DMA_IT_ET  : Early transmit interrupt 
  *     @arg ETH_DMA_IT_RWT : Receive watchdog timeout interrupt 
  *     @arg ETH_DMA_IT_RPS : Receive process stopped interrupt 
  *     @arg ETH_DMA_IT_RBU : Receive buffer unavailable interrupt 
  *     @arg ETH_DMA_IT_R   : Receive interrupt 
  *     @arg ETH_DMA_IT_TU  : Underflow interrupt 
  *     @arg ETH_DMA_IT_RO  : Overflow interrupt 
  *     @arg ETH_DMA_IT_TJT : Transmit jabber timeout interrupt 
  *     @arg ETH_DMA_IT_TBU : Transmit buffer unavailable interrupt 
  *     @arg ETH_DMA_IT_TPS : Transmit process stopped interrupt 
  *     @arg ETH_DMA_IT_T   : Transmit interrupt
  * @param  NewState: new state of the specified ETHERNET DMA interrupts.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_DMAITConfig(uint32_t ETH_DMA_IT, FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_ETH_DMA_IT(ETH_DMA_IT));
  assert_param(IS_FUNCTIONAL_STATE(NewState));  
  
  if (NewState != DISABLE)
  {
    /* Enable the selected ETHERNET DMA interrupts */
    ETH->DMAIER |= ETH_DMA_IT;
  }
  else
  {
    /* Disable the selected ETHERNET DMA interrupts */
    ETH->DMAIER &=(~(uint32_t)ETH_DMA_IT);
  }
}

/**
  * @brief  Checks whether the specified ETHERNET DMA overflow flag is set or not.
  * @param  ETH_DMA_Overflow: specifies the DMA overflow flag to check.
  *   This parameter can be one of the following values:
  *     @arg ETH_DMA_Overflow_RxFIFOCounter : Overflow for FIFO Overflow Counter
  *     @arg ETH_DMA_Overflow_MissedFrameCounter : Overflow for Missed Frame Counter
  * @retval The new state of ETHERNET DMA overflow Flag (SET or RESET).
  */
FlagStatus ETH_GetDMAOverflowStatus(uint32_t ETH_DMA_Overflow)
{
  FlagStatus bitstatus = RESET;
  /* Check the parameters */
  assert_param(IS_ETH_DMA_GET_OVERFLOW(ETH_DMA_Overflow));
  
  if ((ETH->DMAMFBOCR & ETH_DMA_Overflow) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus;
}

/**
  * @brief  Get the ETHERNET DMA Rx Overflow Missed Frame Counter value.
  * @param  None
  * @retval The value of Rx overflow Missed Frame Counter.
  */
uint32_t ETH_GetRxOverflowMissedFrameCounter(void)
{
  return ((uint32_t)((ETH->DMAMFBOCR & ETH_DMAMFBOCR_MFA)>>ETH_DMA_RX_OVERFLOW_MISSEDFRAMES_COUNTERSHIFT));
}

/**
  * @brief  Get the ETHERNET DMA Buffer Unavailable Missed Frame Counter value.
  * @param  None
  * @retval The value of Buffer unavailable Missed Frame Counter.
  */
uint32_t ETH_GetBufferUnavailableMissedFrameCounter(void)
{
  return ((uint32_t)(ETH->DMAMFBOCR) & ETH_DMAMFBOCR_MFC);
}

/**
  * @brief  Get the ETHERNET DMA DMACHTDR register value.
  * @param  None
  * @retval The value of the current Tx desc start address.
  */
uint32_t ETH_GetCurrentTxDescStartAddress(void)
{
  return ((uint32_t)(ETH->DMACHTDR));
}

/**
  * @brief  Get the ETHERNET DMA DMACHRDR register value.
  * @param  None
  * @retval The value of the current Rx desc start address.
  */
uint32_t ETH_GetCurrentRxDescStartAddress(void)
{
  return ((uint32_t)(ETH->DMACHRDR));
}

/**
  * @brief  Get the ETHERNET DMA DMACHTBAR register value.
  * @param  None
  * @retval The value of the current Tx buffer address.
  */
uint32_t ETH_GetCurrentTxBufferAddress(void)
{
  return ((uint32_t)(ETH->DMACHTBAR));
}

/**
  * @brief  Get the ETHERNET DMA DMACHRBAR register value.
  * @param  None
  * @retval The value of the current Rx buffer address.
  */
uint32_t ETH_GetCurrentRxBufferAddress(void)
{
  return ((uint32_t)(ETH->DMACHRBAR));
}

/**
  * @brief  Resumes the DMA Transmission by writing to the DmaTxPollDemand register
  *   (the data written could be anything). This forces  the DMA to resume transmission.
  * @param  None
  * @retval None.
  */
void ETH_ResumeDMATransmission(void)
{
  ETH->DMATPDR = 0;
}

/**
  * @brief  Resumes the DMA Transmission by writing to the DmaRxPollDemand register
  *   (the data written could be anything). This forces the DMA to resume reception.
  * @param  None
  * @retval None.
  */
void ETH_ResumeDMAReception(void)
{
  ETH->DMARPDR = 0;
}

/*---------------------------------  PMT  ------------------------------------*/
/**
  * @brief  Reset Wakeup frame filter register pointer.
  * @param  None
  * @retval None
  */
void ETH_ResetWakeUpFrameFilterRegisterPointer(void)
{  
  /* Resets the Remote Wake-up Frame Filter register pointer to 0x0000 */
  ETH->MACPMTCSR |= ETH_MACPMTCSR_WFFRPR;  
}

/**
  * @brief  Populates the remote wakeup frame registers.
  * @param  Buffer: Pointer on remote WakeUp Frame Filter Register buffer data (8 words).
  * @retval None
  */
void ETH_SetWakeUpFrameFilterRegister(uint32_t *Buffer)
{
  uint32_t i = 0;
  
  /* Fill Remote Wake-up Frame Filter register with Buffer data */
  for(i =0; i<ETH_WAKEUP_REGISTER_LENGTH; i++)
  {
    /* Write each time to the same register */ 
    ETH->MACRWUFFR = Buffer[i];
  }
}

/**
  * @brief  Enables or disables any unicast packet filtered by the MAC address
  *   recognition to be a wake-up frame.
  * @param  NewState: new state of the MAC Global Unicast Wake-Up.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_GlobalUnicastWakeUpCmd(FunctionalState NewState)
{ 
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the MAC Global Unicast Wake-Up */
    ETH->MACPMTCSR |= ETH_MACPMTCSR_GU;  
  }
  else
  {
    /* Disable the MAC Global Unicast Wake-Up */ 
    ETH->MACPMTCSR &= ~ETH_MACPMTCSR_GU;
  }
}

/**
  * @brief  Checks whether the specified ETHERNET PMT flag is set or not.
  * @param  ETH_PMT_FLAG: specifies the flag to check.
  *   This parameter can be one of the following values:
  *     @arg ETH_PMT_FLAG_WUFFRPR : Wake-Up Frame Filter Register Poniter Reset 
  *     @arg ETH_PMT_FLAG_WUFR    : Wake-Up Frame Received 
  *     @arg ETH_PMT_FLAG_MPR     : Magic Packet Received
  * @retval The new state of ETHERNET PMT Flag (SET or RESET).
  */
FlagStatus ETH_GetPMTFlagStatus(uint32_t ETH_PMT_FLAG)
{
  FlagStatus bitstatus = RESET;
  /* Check the parameters */
  assert_param(IS_ETH_PMT_GET_FLAG(ETH_PMT_FLAG));
  
  if ((ETH->MACPMTCSR & ETH_PMT_FLAG) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus;
}

/**
  * @brief  Enables or disables the MAC Wake-Up Frame Detection.
  * @param  NewState: new state of the MAC Wake-Up Frame Detection.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_WakeUpFrameDetectionCmd(FunctionalState NewState)
{ 
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the MAC Wake-Up Frame Detection */
    ETH->MACPMTCSR |= ETH_MACPMTCSR_WFE;  
  }
  else
  {
    /* Disable the MAC Wake-Up Frame Detection */ 
    ETH->MACPMTCSR &= ~ETH_MACPMTCSR_WFE;
  }
}

/**
  * @brief  Enables or disables the MAC Magic Packet Detection.
  * @param  NewState: new state of the MAC Magic Packet Detection.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_MagicPacketDetectionCmd(FunctionalState NewState)
{ 
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the MAC Magic Packet Detection */
    ETH->MACPMTCSR |= ETH_MACPMTCSR_MPE;  
  }
  else
  {
    /* Disable the MAC Magic Packet Detection */ 
    ETH->MACPMTCSR &= ~ETH_MACPMTCSR_MPE;
  }
}

/**
  * @brief  Enables or disables the MAC Power Down.
  * @param  NewState: new state of the MAC Power Down.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_PowerDownCmd(FunctionalState NewState)
{ 
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the MAC Power Down */
    /* This puts the MAC in power down mode */
    ETH->MACPMTCSR |= ETH_MACPMTCSR_PD;  
  }
  else
  {
    /* Disable the MAC Power Down */ 
    ETH->MACPMTCSR &= ~ETH_MACPMTCSR_PD;
  }
}

/*---------------------------------  MMC  ------------------------------------*/
/**
  * @brief  Enables or disables the MMC Counter Freeze.
  * @param  NewState: new state of the MMC Counter Freeze.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_MMCCounterFreezeCmd(FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the MMC Counter Freeze */
    ETH->MMCCR |= ETH_MMCCR_MCF;
  }
  else
  {
    /* Disable the MMC Counter Freeze */
    ETH->MMCCR &= ~ETH_MMCCR_MCF;
  }
}

/**
  * @brief  Enables or disables the MMC Reset On Read.
  * @param  NewState: new state of the MMC Reset On Read.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_MMCResetOnReadCmd(FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the MMC Counter reset on read */
    ETH->MMCCR |= ETH_MMCCR_ROR; 
  }
  else
  {
    /* Disable the MMC Counter reset on read */
    ETH->MMCCR &= ~ETH_MMCCR_ROR;
  }
}

/**
  * @brief  Enables or disables the MMC Counter Stop Rollover.
  * @param  NewState: new state of the MMC Counter Stop Rollover.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_MMCCounterRolloverCmd(FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Disable the MMC Counter Stop Rollover  */
    ETH->MMCCR &= ~ETH_MMCCR_CSR;
  }
  else
  {
    /* Enable the MMC Counter Stop Rollover */
    ETH->MMCCR |= ETH_MMCCR_CSR; 
  }
}

/**
  * @brief  Resets the MMC Counters.
  * @param  None
  * @retval None
  */
void ETH_MMCCountersReset(void)
{
  /* Resets the MMC Counters */
  ETH->MMCCR |= ETH_MMCCR_CR; 
}

/**
  * @brief  Enables or disables the specified ETHERNET MMC interrupts.
  * @param  ETH_MMC_IT: specifies the ETHERNET MMC interrupt sources to be enabled or disabled.
  *   This parameter can be any combination of Tx interrupt or 
  *   any combination of Rx interrupt (but not both)of the following values: 
  *     @arg ETH_MMC_IT_TGF   : When Tx good frame counter reaches half the maximum value 
  *     @arg ETH_MMC_IT_TGFMSC: When Tx good multi col counter reaches half the maximum value 
  *     @arg ETH_MMC_IT_TGFSC : When Tx good single col counter reaches half the maximum value 
  *     @arg ETH_MMC_IT_RGUF  : When Rx good unicast frames counter reaches half the maximum value  
  *     @arg ETH_MMC_IT_RFAE  : When Rx alignment error counter reaches half the maximum value 
  *     @arg ETH_MMC_IT_RFCE  : When Rx crc error counter reaches half the maximum value
  * @param  NewState: new state of the specified ETHERNET MMC interrupts.
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_MMCITConfig(uint32_t ETH_MMC_IT, FunctionalState NewState)
{ 
  /* Check the parameters */
  assert_param(IS_ETH_MMC_IT(ETH_MMC_IT));  
  assert_param(IS_FUNCTIONAL_STATE(NewState));
   
  if ((ETH_MMC_IT & (uint32_t)0x10000000) != (uint32_t)RESET)
  {
    /* Remove egister mak from IT */
    ETH_MMC_IT &= 0xEFFFFFFF;
  
    /* ETHERNET MMC Rx interrupts selected */
    if (NewState != DISABLE)
    {
      /* Enable the selected ETHERNET MMC interrupts */
      ETH->MMCRIMR &=(~(uint32_t)ETH_MMC_IT);
    }
    else
    {
      /* Disable the selected ETHERNET MMC interrupts */
      ETH->MMCRIMR |= ETH_MMC_IT;    
    }
  }
  else
  {
    /* ETHERNET MMC Tx interrupts selected */
    if (NewState != DISABLE)
    {
      /* Enable the selected ETHERNET MMC interrupts */
      ETH->MMCTIMR &=(~(uint32_t)ETH_MMC_IT);
    }
    else
    {
      /* Disable the selected ETHERNET MMC interrupts */
      ETH->MMCTIMR |= ETH_MMC_IT;    
    }  
  }
}

/**
  * @brief  Checks whether the specified ETHERNET MMC IT is set or not.
  * @param  ETH_MMC_IT: specifies the ETHERNET MMC interrupt.
  *   This parameter can be one of the following values:
  *     @arg ETH_MMC_IT_TxFCGC: When Tx good frame counter reaches half the maximum value 
  *     @arg ETH_MMC_IT_TxMCGC: When Tx good multi col counter reaches half the maximum value 
  *     @arg ETH_MMC_IT_TxSCGC: When Tx good single col counter reaches half the maximum value 
  *     @arg ETH_MMC_IT_RxUGFC: When Rx good unicast frames counter reaches half the maximum value  
  *     @arg ETH_MMC_IT_RxAEC : When Rx alignment error counter reaches half the maximum value 
  *     @arg ETH_MMC_IT_RxCEC : When Rx crc error counter reaches half the maximum value 
  * @retval The value of ETHERNET MMC IT (SET or RESET).
  */
ITStatus ETH_GetMMCITStatus(uint32_t ETH_MMC_IT)
{
  ITStatus bitstatus = RESET;
  /* Check the parameters */
  assert_param(IS_ETH_MMC_GET_IT(ETH_MMC_IT)); 
  
  if ((ETH_MMC_IT & (uint32_t)0x10000000) != (uint32_t)RESET)
  {
    /* ETHERNET MMC Rx interrupts selected */
    /* Check if the ETHERNET MMC Rx selected interrupt is enabled and occured */ 
    if ((((ETH->MMCRIR & ETH_MMC_IT) != (uint32_t)RESET)) && ((ETH->MMCRIMR & ETH_MMC_IT) != (uint32_t)RESET))
    {
      bitstatus = SET;
    }
    else
    {
      bitstatus = RESET;
    }
  }
  else
  {
    /* ETHERNET MMC Tx interrupts selected */
    /* Check if the ETHERNET MMC Tx selected interrupt is enabled and occured */  
    if ((((ETH->MMCTIR & ETH_MMC_IT) != (uint32_t)RESET)) && ((ETH->MMCRIMR & ETH_MMC_IT) != (uint32_t)RESET))
    {
      bitstatus = SET;
    }
    else
    {
      bitstatus = RESET;
    }  
  }    
    
  return bitstatus;
}

/**
  * @brief  Get the specified ETHERNET MMC register value.
  * @param  ETH_MMCReg: specifies the ETHERNET MMC register.
  *   This parameter can be one of the following values:
  *     @arg ETH_MMCCR      : MMC CR register 
  *     @arg ETH_MMCRIR     : MMC RIR register 
  *     @arg ETH_MMCTIR     : MMC TIR register 
  *     @arg ETH_MMCRIMR    : MMC RIMR register 
  *     @arg ETH_MMCTIMR    : MMC TIMR register 
  *     @arg ETH_MMCTGFSCCR : MMC TGFSCCR register 
  *     @arg ETH_MMCTGFMSCCR: MMC TGFMSCCR register  
  *     @arg ETH_MMCTGFCR   : MMC TGFCR register
  *     @arg ETH_MMCRFCECR  : MMC RFCECR register 
  *     @arg ETH_MMCRFAECR  : MMC RFAECR register 
  *     @arg ETH_MMCRGUFCR  : MMC RGUFCRregister 
  * @retval The value of ETHERNET MMC Register value.
  */
uint32_t ETH_GetMMCRegister(uint32_t ETH_MMCReg)
{
  /* Check the parameters */
  assert_param(IS_ETH_MMC_REGISTER(ETH_MMCReg));
  
  /* Return the selected register value */
  return (*(__IO uint32_t *)(ETH_MAC_BASE + ETH_MMCReg));
}
/*---------------------------------  PTP  ------------------------------------*/

/**
  * @brief  Updated the PTP block for fine correction with the Time Stamp Addend register value.
  * @param  None
  * @retval None
  */
void ETH_EnablePTPTimeStampAddend(void)
{
  /* Enable the PTP block update with the Time Stamp Addend register value */
  ETH->PTPTSCR |= ETH_PTPTSCR_TSARU;    
}

/**
  * @brief  Enable the PTP Time Stamp interrupt trigger
  * @param  None
  * @retval None
  */
void ETH_EnablePTPTimeStampInterruptTrigger(void)
{
  /* Enable the PTP target time interrupt */
  ETH->PTPTSCR |= ETH_PTPTSCR_TSITE;    
}

/**
  * @brief  Updated the PTP system time with the Time Stamp Update register value.
  * @param  None
  * @retval None
  */
void ETH_EnablePTPTimeStampUpdate(void)
{
  /* Enable the PTP system time update with the Time Stamp Update register value */
  ETH->PTPTSCR |= ETH_PTPTSCR_TSSTU;    
}

/**
  * @brief  Initialize the PTP Time Stamp
  * @param  None
  * @retval None
  */
void ETH_InitializePTPTimeStamp(void)
{
  /* Initialize the PTP Time Stamp */
  ETH->PTPTSCR |= ETH_PTPTSCR_TSSTI;    
}

/**
  * @brief  Selects the PTP Update method
  * @param  UpdateMethod: the PTP Update method
  *   This parameter can be one of the following values:
  *     @arg ETH_PTP_FineUpdate   : Fine Update method 
  *     @arg ETH_PTP_CoarseUpdate : Coarse Update method 
  * @retval None
  */
void ETH_PTPUpdateMethodConfig(uint32_t UpdateMethod)
{
  /* Check the parameters */
  assert_param(IS_ETH_PTP_UPDATE(UpdateMethod));
  
  if (UpdateMethod != ETH_PTP_CoarseUpdate)
  {
    /* Enable the PTP Fine Update method */
    ETH->PTPTSCR |= ETH_PTPTSCR_TSFCU;
  }
  else
  {
    /* Disable the PTP Coarse Update method */
    ETH->PTPTSCR &= (~(uint32_t)ETH_PTPTSCR_TSFCU);
  } 
}

/**
  * @brief  Enables or disables the PTP time stamp for transmit and receive frames.
  * @param  NewState: new state of the PTP time stamp for transmit and receive frames
  *   This parameter can be: ENABLE or DISABLE.
  * @retval None
  */
void ETH_PTPTimeStampCmd(FunctionalState NewState)
{
  /* Check the parameters */
  assert_param(IS_FUNCTIONAL_STATE(NewState));
  
  if (NewState != DISABLE)
  {
    /* Enable the PTP time stamp for transmit and receive frames */
    ETH->PTPTSCR |= ETH_PTPTSCR_TSE;
  }
  else
  {
    /* Disable the PTP time stamp for transmit and receive frames */
    ETH->PTPTSCR &= (~(uint32_t)ETH_PTPTSCR_TSE);
  }
}

/**
  * @brief  Checks whether the specified ETHERNET PTP flag is set or not.
  * @param  ETH_PTP_FLAG: specifies the flag to check.
  *   This parameter can be one of the following values:
  *     @arg ETH_PTP_FLAG_TSARU : Addend Register Update 
  *     @arg ETH_PTP_FLAG_TSITE : Time Stamp Interrupt Trigger Enable 
  *     @arg ETH_PTP_FLAG_TSSTU : Time Stamp Update 
  *     @arg ETH_PTP_FLAG_TSSTI  : Time Stamp Initialize                       
  * @retval The new state of ETHERNET PTP Flag (SET or RESET).
  */
FlagStatus ETH_GetPTPFlagStatus(uint32_t ETH_PTP_FLAG)
{
  FlagStatus bitstatus = RESET;
  /* Check the parameters */
  assert_param(IS_ETH_PTP_GET_FLAG(ETH_PTP_FLAG));
  
  if ((ETH->PTPTSCR & ETH_PTP_FLAG) != (uint32_t)RESET)
  {
    bitstatus = SET;
  }
  else
  {
    bitstatus = RESET;
  }
  return bitstatus;
}

/**
  * @brief  Sets the system time Sub-Second Increment value.
  * @param  SubSecondValue: specifies the PTP Sub-Second Increment Register value.
  * @retval None
  */
void ETH_SetPTPSubSecondIncrement(uint32_t SubSecondValue)
{
  /* Check the parameters */
  assert_param(IS_ETH_PTP_SUBSECOND_INCREMENT(SubSecondValue));
  /* Set the PTP Sub-Second Increment Register */
  ETH->PTPSSIR = SubSecondValue;    
}

/**
  * @brief  Sets the Time Stamp update sign and values.
  * @param  Sign: specifies the PTP Time update value sign.
  *   This parameter can be one of the following values:
  *     @arg ETH_PTP_PositiveTime : positive time value. 
  *     @arg ETH_PTP_NegativeTime : negative time value.  
  * @param  SecondValue: specifies the PTP Time update second value. 
  * @param  SubSecondValue: specifies the PTP Time update sub-second value.
  *   This parameter is a 31 bit value, bit32 correspond to the sign.
  * @retval None
  */
void ETH_SetPTPTimeStampUpdate(uint32_t Sign, uint32_t SecondValue, uint32_t SubSecondValue)
{
  /* Check the parameters */
  assert_param(IS_ETH_PTP_TIME_SIGN(Sign));  
  assert_param(IS_ETH_PTP_TIME_STAMP_UPDATE_SUBSECOND(SubSecondValue)); 
  /* Set the PTP Time Update High Register */
  ETH->PTPTSHUR = SecondValue;
  
  /* Set the PTP Time Update Low Register with sign */
  ETH->PTPTSLUR = Sign | SubSecondValue;   
}

/**
  * @brief  Sets the Time Stamp Addend value.
  * @param  Value: specifies the PTP Time Stamp Addend Register value.
  * @retval None
  */
void ETH_SetPTPTimeStampAddend(uint32_t Value)
{
  /* Set the PTP Time Stamp Addend Register */
  ETH->PTPTSAR = Value;    
}

/**
  * @brief  Sets the Target Time registers values.
  * @param  HighValue: specifies the PTP Target Time High Register value.
  * @param  LowValue: specifies the PTP Target Time Low Register value.
  * @retval None
  */
void ETH_SetPTPTargetTime(uint32_t HighValue, uint32_t LowValue)
{
  /* Set the PTP Target Time High Register */
  ETH->PTPTTHR = HighValue;
  /* Set the PTP Target Time Low Register */
  ETH->PTPTTLR = LowValue;    
}

/**
  * @brief  Get the specified ETHERNET PTP register value.
  * @param  ETH_PTPReg: specifies the ETHERNET PTP register.
  *   This parameter can be one of the following values:
  *     @arg ETH_PTPTSCR  : Sub-Second Increment Register 
  *     @arg ETH_PTPSSIR  : Sub-Second Increment Register 
  *     @arg ETH_PTPTSHR  : Time Stamp High Register
  *     @arg ETH_PTPTSLR  : Time Stamp Low Register 
  *     @arg ETH_PTPTSHUR : Time Stamp High Update Register  
  *     @arg ETH_PTPTSLUR : Time Stamp Low Update Register
  *     @arg ETH_PTPTSAR  : Time Stamp Addend Register
  *     @arg ETH_PTPTTHR  : Target Time High Register 
  *     @arg ETH_PTPTTLR  : Target Time Low Register 
  * @retval The value of ETHERNET PTP Register value.
  */
uint32_t ETH_GetPTPRegister(uint32_t ETH_PTPReg)
{
  /* Check the parameters */
  assert_param(IS_ETH_PTP_REGISTER(ETH_PTPReg));
  
  /* Return the selected register value */
  return (*(__IO uint32_t *)(ETH_MAC_BASE + ETH_PTPReg));
}

/**
  * @brief  Initializes the DMA Tx descriptors in chain mode with PTP.
  * @param  DMATxDescTab: Pointer on the first Tx desc list 
  * @param  DMAPTPTxDescTab: Pointer on the first PTP Tx desc list
  * @param  TxBuff: Pointer on the first TxBuffer list
  * @param  TxBuffCount: Number of the used Tx desc in the list
  * @retval None
  */
void ETH_DMAPTPTxDescChainInit(ETH_DMADESCTypeDef *DMATxDescTab, ETH_DMADESCTypeDef *DMAPTPTxDescTab,
                               uint8_t* TxBuff, uint32_t TxBuffCount)
{
  uint32_t i = 0;
  ETH_DMADESCTypeDef *DMATxDesc;
  
  /* Set the DMATxDescToSet pointer with the first one of the DMATxDescTab list */
  DMATxDescToSet = DMATxDescTab;
  DMAPTPTxDescToSet = DMAPTPTxDescTab;
  /* Fill each DMATxDesc descriptor with the right values */   
  for(i=0; i < TxBuffCount; i++)
  {
    /* Get the pointer on the ith member of the Tx Desc list */
    DMATxDesc = DMATxDescTab+i;
    /* Set Second Address Chained bit and enable PTP */
    DMATxDesc->Status = ETH_DMATxDesc_TCH | ETH_DMATxDesc_TTSE;  
       
    /* Set Buffer1 address pointer */
    DMATxDesc->Buffer1Addr =(uint32_t)(&TxBuff[i*ETH_MAX_PACKET_SIZE]);
    
    /* Initialize the next descriptor with the Next Desciptor Polling Enable */
    if(i < (TxBuffCount-1))
    {
      /* Set next descriptor address register with next descriptor base address */
      DMATxDesc->Buffer2NextDescAddr = (uint32_t)(DMATxDescTab+i+1);
    }
    else
    {
      /* For last descriptor, set next descriptor address register equal to the first descriptor base address */ 
      DMATxDesc->Buffer2NextDescAddr = (uint32_t) DMATxDescTab;  
    }
    /* make DMAPTPTxDescTab points to the same addresses as DMATxDescTab */
    (&DMAPTPTxDescTab[i])->Buffer1Addr = DMATxDesc->Buffer1Addr;
    (&DMAPTPTxDescTab[i])->Buffer2NextDescAddr = DMATxDesc->Buffer2NextDescAddr;
  }
  /* Store on the last DMAPTPTxDescTab desc status record the first list address */
  (&DMAPTPTxDescTab[i-1])->Status = (uint32_t) DMAPTPTxDescTab;

  /* Set Transmit Desciptor List Address Register */
  ETH->DMATDLAR = (uint32_t) DMATxDescTab;
}

/**
  * @brief  Initializes the DMA Rx descriptors in chain mode.
  * @param  DMARxDescTab: Pointer on the first Rx desc list 
  * @param  DMAPTPRxDescTab: Pointer on the first PTP Rx desc list
  * @param  RxBuff: Pointer on the first RxBuffer list
  * @param  RxBuffCount: Number of the used Rx desc in the list
  * @retval None
  */
void ETH_DMAPTPRxDescChainInit(ETH_DMADESCTypeDef *DMARxDescTab, ETH_DMADESCTypeDef *DMAPTPRxDescTab,
                               uint8_t *RxBuff, uint32_t RxBuffCount)
{
  uint32_t i = 0;
  ETH_DMADESCTypeDef *DMARxDesc;
  
  /* Set the DMARxDescToGet pointer with the first one of the DMARxDescTab list */
  DMARxDescToGet = DMARxDescTab; 
  DMAPTPRxDescToGet = DMAPTPRxDescTab;
  /* Fill each DMARxDesc descriptor with the right values */
  for(i=0; i < RxBuffCount; i++)
  {
    /* Get the pointer on the ith member of the Rx Desc list */
    DMARxDesc = DMARxDescTab+i;
    /* Set Own bit of the Rx descriptor Status */
    DMARxDesc->Status = ETH_DMARxDesc_OWN;

    /* Set Buffer1 size and Second Address Chained bit */
    DMARxDesc->ControlBufferSize = ETH_DMARxDesc_RCH | (uint32_t)ETH_MAX_PACKET_SIZE;  
    /* Set Buffer1 address pointer */
    DMARxDesc->Buffer1Addr = (uint32_t)(&RxBuff[i*ETH_MAX_PACKET_SIZE]);
    
    /* Initialize the next descriptor with the Next Desciptor Polling Enable */
    if(i < (RxBuffCount-1))
    {
      /* Set next descriptor address register with next descriptor base address */
      DMARxDesc->Buffer2NextDescAddr = (uint32_t)(DMARxDescTab+i+1); 
    }
    else
    {
      /* For last descriptor, set next descriptor address register equal to the first descriptor base address */ 
      DMARxDesc->Buffer2NextDescAddr = (uint32_t)(DMARxDescTab); 
    }
    /* Make DMAPTPRxDescTab points to the same addresses as DMARxDescTab */
    (&DMAPTPRxDescTab[i])->Buffer1Addr = DMARxDesc->Buffer1Addr;
    (&DMAPTPRxDescTab[i])->Buffer2NextDescAddr = DMARxDesc->Buffer2NextDescAddr;
  }
  /* Store on the last DMAPTPRxDescTab desc status record the first list address */
  (&DMAPTPRxDescTab[i-1])->Status = (uint32_t) DMAPTPRxDescTab;

  /* Set Receive Desciptor List Address Register */
  ETH->DMARDLAR = (uint32_t) DMARxDescTab;  
}

/**
  * @brief  Transmits a packet, from application buffer, pointed by ppkt with Time Stamp values.
  * @param  ppkt: pointer to application packet buffer to transmit.
  * @param  FrameLength: Tx Packet size.
  * @param  PTPTxTab: Pointer on the first PTP Tx table to store Time stamp values.
  * @retval ETH_ERROR: in case of Tx desc owned by DMA
  *         ETH_SUCCESS: for correct transmission
  */
uint32_t ETH_HandlePTPTxPkt(uint8_t *ppkt, uint16_t FrameLength, uint32_t *PTPTxTab)
{
  uint32_t offset = 0, timeout = 0;
  /* Check if the descriptor is owned by the ETHERNET DMA (when set) or CPU (when reset) */
  if((DMATxDescToSet->Status & ETH_DMATxDesc_OWN) != (uint32_t)RESET)
  {
    /* Return ERROR: OWN bit set */
    return ETH_ERROR;
  }
  /* Copy the frame to be sent into memory pointed by the current ETHERNET DMA Tx descriptor */      
  for(offset=0; offset<FrameLength; offset++)
  {
    (*(__IO uint8_t *)((DMAPTPTxDescToSet->Buffer1Addr) + offset)) = (*(ppkt + offset));
  }
  /* Setting the Frame Length: bits[12:0] */
  DMATxDescToSet->ControlBufferSize = (FrameLength & (uint32_t)0x1FFF);
  /* Setting the last segment and first segment bits (in this case a frame is transmitted in one descriptor) */    
  DMATxDescToSet->Status |= ETH_DMATxDesc_LS | ETH_DMATxDesc_FS;
  /* Set Own bit of the Tx descriptor Status: gives the buffer back to ETHERNET DMA */
  DMATxDescToSet->Status |= ETH_DMATxDesc_OWN;
  /* When Tx Buffer unavailable flag is set: clear it and resume transmission */
  if ((ETH->DMASR & ETH_DMASR_TBUS) != (uint32_t)RESET)
  {
    /* Clear TBUS ETHERNET DMA flag */
    ETH->DMASR = ETH_DMASR_TBUS;
    /* Resume DMA transmission*/
    ETH->DMATPDR = 0;
  }
  /* Wait for ETH_DMATxDesc_TTSS flag to be set */
  do
  {
    timeout++;
  } while (!(DMATxDescToSet->Status & ETH_DMATxDesc_TTSS) && (timeout < 0xFFFF));
  /* Return ERROR in case of timeout */
  if(timeout == PHY_READ_TO)
  {
    return ETH_ERROR;
  }
  /* Clear the DMATxDescToSet status register TTSS flag */
  DMATxDescToSet->Status &= ~ETH_DMATxDesc_TTSS;
  *PTPTxTab++ = DMATxDescToSet->Buffer1Addr;
  *PTPTxTab = DMATxDescToSet->Buffer2NextDescAddr;
  /* Update the ENET DMA current descriptor */
  /* Chained Mode */
  if((DMATxDescToSet->Status & ETH_DMATxDesc_TCH) != (uint32_t)RESET)
  {  
    /* Selects the next DMA Tx descriptor list for next buffer read */ 
    DMATxDescToSet = (ETH_DMADESCTypeDef*) (DMAPTPTxDescToSet->Buffer2NextDescAddr);
    if(DMAPTPTxDescToSet->Status != 0)
    { 
      DMAPTPTxDescToSet = (ETH_DMADESCTypeDef*) (DMAPTPTxDescToSet->Status);
    }
    else
    {
      DMAPTPTxDescToSet++;
    }
  }
  else /* Ring Mode */
  {  
    if((DMATxDescToSet->Status & ETH_DMATxDesc_TER) != (uint32_t)RESET)
    {
      /* Selects the next DMA Tx descriptor list for next buffer read: this will
         be the first Tx descriptor in this case */
      DMATxDescToSet = (ETH_DMADESCTypeDef*) (ETH->DMATDLAR); 
      DMAPTPTxDescToSet = (ETH_DMADESCTypeDef*) (ETH->DMATDLAR);
    }
    else
    {
      /* Selects the next DMA Tx descriptor list for next buffer read */
      DMATxDescToSet = (ETH_DMADESCTypeDef*) ((uint32_t)DMATxDescToSet + 0x10 + ((ETH->DMABMR & ETH_DMABMR_DSL) >> 2));      
      DMAPTPTxDescToSet = (ETH_DMADESCTypeDef*) ((uint32_t)DMAPTPTxDescToSet + 0x10 + ((ETH->DMABMR & ETH_DMABMR_DSL) >> 2));      
    }
  }
  /* Return SUCCESS */
  return ETH_SUCCESS;
}

/**
  * @brief  Receives a packet and copies it to memory pointed by ppkt with Time Stamp values.
  * @param  ppkt: pointer to application packet receive buffer.  
  * @param  PTPRxTab: Pointer on the first PTP Rx table to store Time stamp values.
  * @retval ETH_ERROR: if there is error in reception
  *         framelength: received packet size if packet reception is correct
  */
uint32_t ETH_HandlePTPRxPkt(uint8_t *ppkt, uint32_t *PTPRxTab)
{
  uint32_t offset = 0, framelength = 0;
  /* Check if the descriptor is owned by the ENET or CPU */
  if((DMARxDescToGet->Status & ETH_DMARxDesc_OWN) != (uint32_t)RESET)
  {
    /* Return error: OWN bit set */
    return ETH_ERROR;
  }
  if(((DMARxDescToGet->Status & ETH_DMARxDesc_ES) == (uint32_t)RESET) &&
     ((DMARxDescToGet->Status & ETH_DMARxDesc_LS) != (uint32_t)RESET) &&
     ((DMARxDescToGet->Status & ETH_DMARxDesc_FS) != (uint32_t)RESET))
  {
    /* Get the Frame Length of the received packet: substruct 4 bytes of the CRC */
    framelength = ((DMARxDescToGet->Status & ETH_DMARxDesc_FL) >> ETH_DMARXDESC_FRAME_LENGTHSHIFT) - 4;
    /* Copy the received frame into buffer from memory pointed by the current ETHERNET DMA Rx descriptor */ 
    for(offset=0; offset<framelength; offset++)
    {
      (*(ppkt + offset)) = (*(__IO uint8_t *)((DMAPTPRxDescToGet->Buffer1Addr) + offset));
    }
  }
  else
  {
    /* Return ERROR */
    framelength = ETH_ERROR;
  }
  /* When Rx Buffer unavailable flag is set: clear it and resume reception */
  if ((ETH->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET)
  {
    /* Clear RBUS ETHERNET DMA flag */
    ETH->DMASR = ETH_DMASR_RBUS;
    /* Resume DMA reception */
    ETH->DMARPDR = 0;
  }
  *PTPRxTab++ = DMARxDescToGet->Buffer1Addr;
  *PTPRxTab = DMARxDescToGet->Buffer2NextDescAddr;
  /* Set Own bit of the Rx descriptor Status: gives the buffer back to ETHERNET DMA */
  DMARxDescToGet->Status |= ETH_DMARxDesc_OWN;
  /* Update the ETHERNET DMA global Rx descriptor with next Rx decriptor */
  /* Chained Mode */
  if((DMARxDescToGet->ControlBufferSize & ETH_DMARxDesc_RCH) != (uint32_t)RESET)
  {
    /* Selects the next DMA Rx descriptor list for next buffer read */
    DMARxDescToGet = (ETH_DMADESCTypeDef*) (DMAPTPRxDescToGet->Buffer2NextDescAddr);
    if(DMAPTPRxDescToGet->Status != 0)
    {
      DMAPTPRxDescToGet = (ETH_DMADESCTypeDef*) (DMAPTPRxDescToGet->Status);
    }
    else
    {
      DMAPTPRxDescToGet++;
    }
  }
  else /* Ring Mode */
  {
    if((DMARxDescToGet->ControlBufferSize & ETH_DMARxDesc_RER) != (uint32_t)RESET)
    {
      /* Selects the first DMA Rx descriptor for next buffer to read: last Rx descriptor was used */
      DMARxDescToGet = (ETH_DMADESCTypeDef*) (ETH->DMARDLAR);
    }
    else
    {
      /* Selects the next DMA Rx descriptor list for next buffer to read */
      DMARxDescToGet = (ETH_DMADESCTypeDef*) ((uint32_t)DMARxDescToGet + 0x10 + ((ETH->DMABMR & ETH_DMABMR_DSL) >> 2));      
    }
  }
  /* Return Frame Length/ERROR */
  return (framelength);
}

#ifndef USE_Delay
/**
  * @brief  Inserts a delay time.
  * @param  nCount: specifies the delay time length.
  * @retval None
  */
static void ETH_Delay(__IO uint32_t nCount)
{
  __IO uint32_t index = 0; 
  for(index = nCount; index != 0; index--)
  {
  }
}
#endif /* USE_Delay*/

/**
  * @}
  */

/**
  * @}
  */

/******************* (C) COPYRIGHT 2009 STMicroelectronics *****END OF FILE****/
