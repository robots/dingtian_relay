#include "platform.h"

#include "lwip/opt.h"
#include "lwip/mem.h"
#include "netif/etharp.h"
#include "ethernetif.h"

#include <string.h>

#define IFNAME0 's'
#define IFNAME1 't'

#define ETH_DMARxDesc_FrameLengthShift           16

#define ETH_RXBUFNB        10
#define ETH_TXBUFNB        10

#define ETH_RX_BUF_SIZE    ETH_MAX_PACKET_SIZE
#define ETH_TX_BUF_SIZE    ETH_MAX_PACKET_SIZE

ETH_DMADESCTypeDef  DMARxDscrTab[ETH_RXBUFNB] __attribute__ ((aligned (4), section(".lwipram"))); /* Ethernet Rx MA Descriptor */
ETH_DMADESCTypeDef  DMATxDscrTab[ETH_TXBUFNB] __attribute__ ((aligned (4), section(".lwipram"))); /* Ethernet Tx DMA Descriptor */

struct eth_ll_t {
  struct eth_ll_t *next;
  struct pbuf *p;
  volatile ETH_DMADESCTypeDef *d;
};

struct eth_ll_t eth_tx_pbufs[ETH_TXBUFNB];
struct eth_ll_t *eth_tx_pbuf_get;
struct eth_ll_t *eth_tx_pbuf_set;
struct eth_ll_t eth_rx_pbufs[ETH_RXBUFNB];
struct eth_ll_t *eth_rx_pbuf_get;
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
  {
    uint32_t i;

    for (i = 0; i < ETH_TXBUFNB; i++)
    {
      DMATxDscrTab[i].Status = ETH_DMATxDesc_TCH;

      DMATxDscrTab[i].Buffer1Addr = 0;
      DMATxDscrTab[i].ControlBufferSize = 0;
      eth_tx_pbufs[i].p = NULL;
      eth_tx_pbufs[i].d = NULL;

      if (i < (ETH_TXBUFNB - 1)) {
        eth_tx_pbufs[i].next = &eth_tx_pbufs[i+1];
        DMATxDscrTab[i].Buffer2NextDescAddr = (uint32_t)&DMATxDscrTab[i+1];
      } else {
        eth_tx_pbufs[i].next = &eth_tx_pbufs[0];
        DMATxDscrTab[i].Buffer2NextDescAddr = (uint32_t)&DMATxDscrTab[0];
      }
    }

    ETH->DMATDLAR = (uint32_t)&DMATxDscrTab[0];
    DMATxDescToSet = &DMATxDscrTab[0];
    eth_tx_pbuf_get = &eth_tx_pbufs[0];
    eth_tx_pbuf_set = &eth_tx_pbufs[0];
  }

  /* Initialize Rx Descriptors list: Chain Mode  */
  {
    uint32_t i;
    struct pbuf *p;

    for (i = 0; i < ETH_RXBUFNB; i++) {
      DMARxDscrTab[i].Status = ETH_DMARxDesc_OWN;

      DMARxDscrTab[i].ControlBufferSize = ETH_DMARxDesc_RCH | (uint32_t)PBUF_POOL_BUFSIZE;
      p = pbuf_alloc(PBUF_RAW, PBUF_POOL_BUFSIZE, PBUF_POOL);

      LWIP_ASSERT("eth: pbuf allocated", p != NULL);

      DMARxDscrTab[i].Buffer1Addr = (uint32_t)p->payload;

      /* Initialize the next descriptor with the Next Desciptor Polling Enable */
      if (i < (ETH_RXBUFNB - 1)) {
        DMARxDscrTab[i].Buffer2NextDescAddr = (uint32_t)&DMARxDscrTab[i+1];
      } else {
        DMARxDscrTab[i].Buffer2NextDescAddr = (uint32_t)&DMARxDscrTab[0];
      }

      eth_rx_pbufs[i].p = p;
      eth_rx_pbufs[i].d = &DMARxDscrTab[i];
			if (i < (ETH_RXBUFNB - 1)) {
				eth_rx_pbufs[i].next = &eth_rx_pbufs[i+1];
			} else {
				eth_rx_pbufs[i].next = &eth_rx_pbufs[0];
			}
    }
    ETH->DMARDLAR = (uint32_t)&DMARxDscrTab[0];
    DMARxDescToGet = &DMARxDscrTab[0];
		eth_rx_pbuf_get = &eth_rx_pbufs[0];
  }

  
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

static void low_level_free_pbuf(void)
{
  struct eth_ll_t *pd;
//  for (uint32_t i = 0; i < ETH_TXBUFNB; i++) {
//    pd = eth_tx_pbufs[i];

  pd = eth_tx_pbuf_get;
  while (1) {
    // nothing waiting, finish
    if (!pd->p || !pd->d) {
      break;
    }

    uint8_t cnt = pbuf_clen(pd->p);
    __IO ETH_DMADESCTypeDef *DmaTxDesc = pd->d;

    // check "cnt" dma descriptors for "OWN"
    while (!(DmaTxDesc->Status & ETH_DMATxDesc_OWN)) {
      cnt--;
      DmaTxDesc = (ETH_DMADESCTypeDef *)DmaTxDesc->Buffer2NextDescAddr;

      // all buffers are our -> pbuf sent, lets free it
      if (cnt == 0) {
        pbuf_free(pd->p);

        pd->d = NULL;
        pd->p = NULL;

        eth_tx_pbuf_get = eth_tx_pbuf_get->next;
        break;
      }
    }

    // not our in all descriptors? this pbuf will be dealt later.
    if (cnt) {
      break;
    }

    pd = pd->next;
  }

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

  // first free existing pbufs
  low_level_free_pbuf();

  DmaTxDesc_First = DmaTxDesc = DMATxDescToSet;

  // we want to own the pbuf chain
  pbuf_ref(p);

  // save this pbuf
  eth_tx_pbuf_set->p = p;
  eth_tx_pbuf_set->d = DmaTxDesc;
  eth_tx_pbuf_set = eth_tx_pbuf_set->next;

  /* copy frame from pbufs to driver buffers */
  for (q = p; q != NULL; q = q->next) {
    // unlikely case
    while (DmaTxDesc->Status & ETH_DMATxDesc_OWN);

    DmaTxDesc->Status &= ~(ETH_DMATxDesc_LS | ETH_DMATxDesc_FS);

    DmaTxDesc->ControlBufferSize = (q->len & ETH_DMATxDesc_TBS1);
    DmaTxDesc->Buffer1Addr = (uint32_t)q->payload;

    if (q->next) {
      DmaTxDesc = (ETH_DMADESCTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);

      if (DmaTxDesc->Status & ETH_DMATxDesc_OWN) {
        errval = ERR_USE;
        goto error;
      }
    }
  }

  // set first/last segment bit
  DmaTxDesc_First->Status |= ETH_DMATxDesc_FS;
  DmaTxDesc->Status |= ETH_DMATxDesc_LS;

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
  if (ETH->DMASR & ETH_DMASR_TBUS) {
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
	struct eth_ll_t *ecur, *elast;
  __IO ETH_DMADESCTypeDef *DMARxDesc;
  __IO ETH_DMADESCTypeDef *DMARxDesc_Last;

  if (DMARxDescToGet->Status & ETH_DMARxDesc_OWN) {
    goto exit;
  }

  DMARxDesc_Last = DMARxDesc = DMARxDescToGet;
	elast = ecur = eth_rx_pbuf_get;

  while (!(DMARxDesc_Last->Status & ETH_DMARxDesc_LS)) {
    DMARxDesc_Last = (ETH_DMADESCTypeDef *)DMARxDesc_Last->Buffer2NextDescAddr;
		elast = elast->next;
    if (DMARxDesc_Last->Status & ETH_DMARxDesc_OWN) {
      // this buffer is not ours yet, and we didnt find Last segment
      // exit and we will fetch the frame in next try
      goto exit;
    }
  }

  len = (DMARxDesc_Last->Status & ETH_DMARxDesc_FL) >> ETH_DMARxDesc_FrameLengthShift;
  len -= 4; // remove crc

  while (1) {
    struct pbuf *tp;

    LWIP_ASSERT("eth: ecur->d == DMARxDesc", ecur->d == DMARxDesc);

		q = ecur->p;
    //q = (struct pbuf *) (DMARxDesc->Buffer1Addr - LWIP_MEM_ALIGN_SIZE(sizeof(struct pbuf)));

    q->len = q->tot_len = MIN(PBUF_POOL_BUFSIZE, len);
    len -= q->len;

    if (p == NULL) {
      p = q;
    } else {
      pbuf_cat(p, q);
    }

    tp = pbuf_alloc(PBUF_RAW, PBUF_POOL_BUFSIZE, PBUF_POOL);
    LWIP_ASSERT("eth: buffer allocated", tp != NULL);

		// save new pbuf into ll
		ecur->p = tp;

    DMARxDesc->Buffer1Addr = (uint32_t)tp->payload;
    DMARxDesc->ControlBufferSize = ETH_DMARxDesc_RCH | (uint32_t)PBUF_POOL_BUFSIZE;

    if (DMARxDesc->Status & ETH_DMARxDesc_LS) {
      // we reached last segment
      	break;
    }

    DMARxDesc->Status |= ETH_DMARxDesc_OWN;
    DMARxDesc = (ETH_DMADESCTypeDef *)DMARxDesc->Buffer2NextDescAddr;
		ecur = ecur->next;
  }

  // release last descriptor
  DMARxDesc->Status |= ETH_DMARxDesc_OWN;
  DMARxDescToGet = (ETH_DMADESCTypeDef *)DMARxDesc_Last->Buffer2NextDescAddr;
	eth_rx_pbuf_get = elast->next;

exit:
  /* When Rx Buffer unavailable flag is set: clear it and resume reception */
  if (ETH->DMASR & ETH_DMASR_RBUS) {
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

  // first free existing pbufs
  low_level_free_pbuf();

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
