/**
  ******************************************************************************
  * @file    LwIP/LwIP_IAP/Src/ethernetif.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    26-February-2014
  * @brief   This file implements Ethernet network interface drivers for lwIP
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2014 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "platform.h"

#include "lwip/opt.h"
#include "netif/etharp.h"
#include "ethernetif.h"

#include <string.h>

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Network interface name */
#define IFNAME0 's'
#define IFNAME1 't'

#define  ETH_DMARxDesc_FrameLengthShift           16

#define ETH_RXBUFNB        6
#define ETH_TXBUFNB        4

#define ETH_RX_BUF_SIZE    ETH_MAX_PACKET_SIZE
#define ETH_TX_BUF_SIZE    ETH_MAX_PACKET_SIZE

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
ETH_DMADESCTypeDef  DMARxDscrTab[ETH_RXBUFNB] __attribute__ ((aligned (4), section(".lwipram"))); /* Ethernet Rx MA Descriptor */
ETH_DMADESCTypeDef  DMATxDscrTab[ETH_TXBUFNB] __attribute__ ((aligned (4), section(".lwipram"))); /* Ethernet Tx DMA Descriptor */
uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __attribute__ ((aligned (4), section(".lwipram"))); /* Ethernet Receive Buffer */
uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __attribute__ ((aligned (4), section(".lwipram"))); /* Ethernet Transmit Buffer */

extern ETH_DMADESCTypeDef  *DMATxDescToSet;
extern ETH_DMADESCTypeDef  *DMARxDescToGet;


/*******************************************************************************
                       LL Driver Interface ( LwIP stack --> ETH) 
*******************************************************************************/
/**
  * @brief In this function, the hardware should be initialized.
  * Called from ethernetif_init().
  *
  * @param netif the already initialized lwip network interface structure
  *        for this ethernetif
  */
static void low_level_init(struct netif *netif)
{ 
  /* set MAC hardware address length */
  netif->hwaddr_len = ETHARP_HWADDR_LEN;
  
  /* set MAC hardware address */
  ETH_GetMACAddress(ETH_MAC_Address0, netif->hwaddr);
  
  /* maximum transfer unit */
  netif->mtu = 1500;

  /* Initialize Tx Descriptors list: Chain Mode */
  ETH_DMATxDescChainInit(DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);
     
  /* Initialize Rx Descriptors list: Chain Mode  */
  ETH_DMARxDescChainInit(DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);
  
  /* device capabilities */
  /* don't set NETIF_FLAG_ETHARP if this device is not an ethernet one */
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

#if STM32_ETH_INTERRUPTS
	/* Enable Ethernet Rx interrrupt */
	for(int i=0; i<ETH_RXBUFNB; i++) {
		ETH_DMARxDescReceiveITConfig(&DMARxDscrTab[i], ENABLE);
	}
#endif

#ifdef CHECKSUM_BY_HARDWARE
	/* Enable the checksum insertion for the Tx frames */
	for(int i = 0; i < ETH_TXBUFNB; i++) {
		ETH_DMATxDescChecksumInsertionConfig(&DMATxDscrTab[i], ETH_DMATxDesc_ChecksumTCPUDPICMPFull);
	}
#endif
}

/**
  * @brief This function should do the actual transmission of the packet. The packet is
  * contained in the pbuf that is passed to the function. This pbuf
  * might be chained.
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
  * @return ERR_OK if the packet could be sent
  *         an err_t value if the packet couldn't be sent
  *
  * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
  *       strange results. You might consider waiting for space in the DMA queue
  *       to become availale since the stack doesn't retry to send a packet
  *       dropped because of memory failure (except for the TCP timers).
  */
//static
err_t low_level_output(struct netif *netif, struct pbuf *p)
{
	(void)netif;
  err_t errval;
  struct pbuf *q;
  __IO ETH_DMADESCTypeDef *DmaTxDesc;
  __IO ETH_DMADESCTypeDef *DmaTxDesc_First;
  uint32_t framelength = 0;
  uint32_t dma_buf_offset = 0;
  uint32_t byteslefttocopy = 0;
  uint32_t payloadoffset = 0;

  DmaTxDesc_First = DmaTxDesc = DMATxDescToSet;
  dma_buf_offset = 0;

	DmaTxDesc->Status &= ~(ETH_DMATxDesc_LS);
	DmaTxDesc->Status |= ETH_DMATxDesc_FS;
  
  /* copy frame from pbufs to driver buffers */
  for(q = p; q != NULL; q = q->next)
  {
    /* Is this buffer available? If not, goto error */
    if((DmaTxDesc->Status & ETH_DMATxDesc_OWN) != (uint32_t)RESET)
    {
      errval = ERR_USE;
      goto error;
    }

    /* Get bytes in current lwIP buffer */
    byteslefttocopy = q->len;
    payloadoffset = 0;
    
    /* Check if the length of data to copy is bigger than Tx buffer size*/
    if ((byteslefttocopy + dma_buf_offset) > ETH_TX_BUF_SIZE)
    {
      /* Copy data to Tx buffer*/
      memcpy( (uint8_t*)((uint8_t*)DmaTxDesc->Buffer1Addr + dma_buf_offset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), (ETH_TX_BUF_SIZE - dma_buf_offset) );
			DmaTxDesc->ControlBufferSize = (ETH_TX_BUF_SIZE & ETH_DMATxDesc_TBS1);
      
      /* Point to next descriptor */
      DmaTxDesc = (ETH_DMADESCTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);
			DmaTxDesc->Status &= ~(ETH_DMATxDesc_FS|ETH_DMATxDesc_LS);
      
      /* Check if the buffer is available */
      if((DmaTxDesc->Status & ETH_DMATxDesc_OWN) != (uint32_t)RESET)
      {
        errval = ERR_USE;
        goto error;
      }
      
      byteslefttocopy = byteslefttocopy - (ETH_TX_BUF_SIZE - dma_buf_offset);
      payloadoffset = payloadoffset + (ETH_TX_BUF_SIZE - dma_buf_offset);
      framelength = framelength + (ETH_TX_BUF_SIZE - dma_buf_offset);
      dma_buf_offset = 0;
    }
    
    /* Copy the remaining bytes */
    memcpy( (uint8_t*)((uint8_t*)DmaTxDesc->Buffer1Addr + dma_buf_offset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), byteslefttocopy );
    dma_buf_offset = dma_buf_offset + byteslefttocopy;
    framelength = framelength + byteslefttocopy;
  }

	// set last segment bit
	DmaTxDesc->Status |= ETH_DMATxDesc_LS;

	// set correct buffer length
	DmaTxDesc->ControlBufferSize = (dma_buf_offset & ETH_DMATxDesc_TBS1);
 
	// Set OWN bit on all modified DMA descs
	while (DmaTxDesc_First != DmaTxDesc) {
		DmaTxDesc_First->Status |= ETH_DMATxDesc_OWN;
		DmaTxDesc_First = (ETH_DMADESCTypeDef *)(DmaTxDesc_First->Buffer2NextDescAddr);
	}
	DmaTxDesc_First->Status |= ETH_DMATxDesc_OWN;

	DMATxDescToSet = (ETH_DMADESCTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);
  errval = ERR_OK;

  
error:
  
  /* When Transmit Underflow flag is set, clear it and issue a Transmit Poll Demand to resume transmission */
  if ((ETH->DMASR & ETH_DMASR_TBUS) != (uint32_t)RESET)
  {
    /* Clear TUS ETHERNET DMA flag */
    ETH->DMASR = ETH_DMASR_TUS;
    
    /* Resume DMA transmission*/
    ETH->DMATPDR = 0;
  }
  return errval;
}

/**
  * @brief Should allocate a pbuf and transfer the bytes of the incoming
  * packet from the interface into the pbuf.
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @return a pbuf filled with the received packet (including MAC header)
  *         NULL on memory error
  */
//static
struct pbuf * low_level_input(struct netif *netif)
{
	(void)netif;
  struct pbuf *p = NULL;
  struct pbuf *q;
  uint16_t len;
//  uint8_t *buffer;
  __IO ETH_DMADESCTypeDef *DMARxDesc;
  __IO ETH_DMADESCTypeDef *DMARxDesc_Last;
  uint32_t bufferoffset = 0;
  uint32_t payloadoffset = 0;
  uint32_t byteslefttocopy = 0;
  
	if (DMARxDescToGet->Status & ETH_DMARxDesc_OWN) {
		goto exit;
	}
	
	DMARxDesc_Last = DMARxDesc = DMARxDescToGet;
	len = (DMARxDesc_Last->Status & ETH_DMARxDesc_FL) >> ETH_DMARxDesc_FrameLengthShift;

	while (!(DMARxDesc_Last->Status & ETH_DMARxDesc_LS)) {
		DMARxDesc_Last = (ETH_DMADESCTypeDef *)DMARxDesc_Last->Buffer2NextDescAddr;
		if (DMARxDesc_Last->Status & ETH_DMARxDesc_OWN) {
			// this buffer is not ours yet, and we didnt find Last segment
			// exit and we will fetch the frame in next try
			goto exit;
		}
	}
	len -= 4; // remove crc
	//buffer = (uint8_t *)(DMARxDesc->Buffer1Addr);

  if (len > 0)
  {
    /* We allocate a pbuf chain of pbufs from the Lwip buffer pool */
    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  }
  
  if (p != NULL)
  {

    bufferoffset = 0;
		payloadoffset = 0;
		byteslefttocopy = len;
		q = p;

		do {
			// copy as much as we can
			// q->len - payloadoffset ~ space left in pbuf
			// byteslefttocopy ~ number of bytes left to copy to complete frame
			uint16_t tocopy = MIN(q->len - payloadoffset, byteslefttocopy);

			// check if there is enough data in dmabuf
			if (tocopy + bufferoffset > ETH_RX_BUF_SIZE) {
				tocopy = ETH_RX_BUF_SIZE - bufferoffset;
			}

			// copy
			memcpy((uint8_t*)((uint8_t*)q->payload + payloadoffset), (uint8_t*)((uint8_t*)DMARxDesc->Buffer1Addr + bufferoffset), tocopy);

			// update offsets
			byteslefttocopy -= tocopy;
			bufferoffset += tocopy;
			payloadoffset += tocopy;

			if (byteslefttocopy == 0)
				break;

			// pbuf is full, advance to next
			if (payloadoffset >= q->len) {
				if (q->next == NULL) {
					// this should not happen.
					break;
				}
				q = q->next;
				payloadoffset = 0;
			}

			// dma buff is full, adcance to next
			if (bufferoffset >= ETH_RX_BUF_SIZE) {
				if (DMARxDesc == DMARxDesc_Last) {
					// this should not happen.
					break;
				}

				DMARxDesc->Status |= ETH_DMARxDesc_OWN;
				DMARxDesc = (ETH_DMADESCTypeDef *)DMARxDesc->Buffer2NextDescAddr;
				bufferoffset = 0;
			}

		} while (1);

		// release last descriptor
		DMARxDesc->Status |= ETH_DMARxDesc_OWN;
		DMARxDescToGet = (ETH_DMADESCTypeDef *)DMARxDesc_Last->Buffer2NextDescAddr;
  }

exit:
  /* When Rx Buffer unavailable flag is set: clear it and resume reception */
  if ((ETH->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET)  
  {
    /* Clear RBUS ETHERNET DMA flag */
    ETH->DMASR = ETH_DMASR_RBUS;
    /* Resume DMA reception */
    ETH->DMARPDR = 0;
  }
  return p;
}

/**
  * @brief This function should be called when a packet is ready to be read
  * from the interface. It uses the function low_level_input() that
  * should handle the actual reception of bytes from the network
  * interface. Then the type of the received packet is determined and
  * the appropriate input function is called.
  *
  * @param netif the lwip network interface structure for this ethernetif
  */
void ethernetif_input(struct netif *netif)
{
  err_t err;
  struct pbuf *p;
  
  /* move received packet into a new pbuf */
  p = low_level_input(netif);
    
  /* no packet could be read, silently ignore this */
  if (p == NULL)
		return;
    
  /* entry point to the LwIP stack */
  err = netif->input(p, netif);
    
  if (err != ERR_OK)
  {
    LWIP_DEBUGF(NETIF_DEBUG, ("ethernetif_input: IP input error\n"));
    pbuf_free(p);
    p = NULL;
  }
}

/**
  * @brief Should be called at the beginning of the program to set up the
  * network interface. It calls the function low_level_init() to do the
  * actual setup of the hardware.
  *
  * This function should be passed as a parameter to netif_add().
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @return ERR_OK if the loopif is initialized
  *         ERR_MEM if private data couldn't be allocated
  *         any other err_t on error
  */
err_t ethernetif_init(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));
  
#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;
  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  netif->output = etharp_output;
  netif->linkoutput = low_level_output;

  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}

/**
  * @brief  This function sets the netif link status.
  * @param  netif: the network interface
  * @retval None
  */
void ethernetif_set_link(struct netif *netif)
{
  uint32_t regvalue = 0;
  
  /* Read PHY_MISR*/
  regvalue = ETH_ReadPHYRegister(0, PHY_ISR);
  
  /* Check whether the link interrupt has occurred or not */
  if (regvalue & PHY_INT_LINK) {
    regvalue = ETH_ReadPHYRegister(0, PHY_BSR);
    
    /* Check whether the link is up or down*/
    if (regvalue & PHY_BSR_LINK) {
      netif_set_link_up(netif);
    } else {
      netif_set_link_down(netif);
    }
  }
}

void ethernetif_setmac(uint8_t* macadd)
{
  ETH_MACAddressConfig(ETH_MAC_Address0, macadd);  
}

/**
  * @brief  Link callback function, this function is called on change of link status
  *         to update low level driver configuration.
* @param  netif: The network interface
  * @retval None
  */
void ethernetif_update_config(struct netif *netif)
{
	__IO uint32_t timeout = 0;
	uint32_t regvalue = 0;
	ETH_InitTypeDef *ETH_Struct = (ETH_InitTypeDef *)(netif->state);

	if(netif_is_link_up(netif)) {
		/* Restart the auto-negotiation */
		if (ETH_Struct->ETH_AutoNegotiation) {

			/* Wait until the auto-negotiation will be completed */
			do {
				timeout++;
				regvalue = ETH_ReadPHYRegister(0, PHY_SR);
			} while (!(regvalue & PHY_SR_AUTONEG_DONE) && (timeout < PHY_READ_TO));

			if (timeout == PHY_READ_TO) {
				goto error;
			}

			/* Reset Timeout counter */
			timeout = 0;

			/* Configure the MAC with the Duplex Mode fixed by the auto-negotiation process */
			if((regvalue & PHY_DUPLEX_STATUS) != (uint32_t)RESET) {
				/* Set Ethernet duplex mode to Full-duplex following the auto-negotiation */
				ETH->MACCR |= ETH_Mode_FullDuplex;
			} else {
				/* Set Ethernet duplex mode to Half-duplex following the auto-negotiation */
				ETH->MACCR &= ~(ETH_Mode_FullDuplex);
			}
			/* Configure the MAC with the speed fixed by the auto-negotiation process */
			if(regvalue & PHY_SPEED_STATUS) {
				/* Set Ethernet speed to 10M following the auto-negotiation */
				ETH->MACCR &= ~(ETH_Speed_100M);
			} else {
				/* Set Ethernet speed to 100M following the auto-negotiation */
				ETH->MACCR |= ETH_Speed_100M;
			}
		} else {
			/* AutoNegotiation Disable */
error:
			/* Check parameters */
			/* Set MAC Speed and Duplex Mode to PHY */
			regvalue = ETH_ReadPHYRegister(0, PHY_BCR);
			regvalue &= ~(PHY_BCR_AUTONEG | PHY_BCR_DUPLEX | PHY_BCR_SPEED); // disable autoneg
			regvalue |= (ETH->MACCR & ETH_Mode_FullDuplex)?PHY_BCR_DUPLEX:0;
			regvalue |= (ETH->MACCR & ETH_Speed_100M)?PHY_BCR_SPEED:0;
			ETH_WritePHYRegister(0, PHY_BCR, regvalue);
		}

		/* Restart MAC interface */
		ETH_Start();
	} else {
		/* Stop MAC interface */
		ETH_Stop();
	}

	ethernetif_notify_conn_changed(netif);
}

/**
  * @brief  This function notify user about link status changement.
  * @param  netif: the network interface
  * @retval None
  */
void __attribute__ ((weak)) ethernetif_notify_conn_changed(struct netif *netif)
{
	(void)netif;
  /* NOTE : This is function clould be implemented in user file 
            when the callback is needed,
  */  
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
