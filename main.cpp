/*
 * @Author: Rex 
 * @Date: 2018-09-25 10:58:51 
 * @Last Modified by:  
 * @Last Modified time: 2018-09-28 16:42:05
 */
#include "test.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <linux/if_packet.h>
#include <netinet/if_ether.h>
#include <iostream>
#include <fstream>
#include <time.h>
#include "OrderbookData.h"
#include "TradeData.h"
#include "ReferenceData.h"
#include "Instrument.h"
#define BUFF_SIZE 2048
static const char *g_szIfName = "ens3f1np1"; // 网卡接口

typedef struct XdpPacketHeader
{
    unsigned short mPktSize;
    unsigned char mMsgCount;
    unsigned char mFiller;
    unsigned int mSeqNum;
    unsigned long long mSendTime;
} PacketHeader;

typedef struct XdpMessageHeader
{
    unsigned short mMsgSize;
    unsigned short mMsgType;
} MessageHeader;

void ProcessMessageHeader(char *buf, int msgCount);
void AddOrder(char *buf, uint16_t offset, uint16_t len);
void ModifyOrder(char *buf, uint16_t offset, uint16_t len);
void DeleteOrder(char *buf, uint16_t offset, uint16_t len);
void ClearOrder(char *buf, uint16_t offset, uint16_t len);
void Trade(char *buf, uint16_t offset, uint16_t len);
void TradeAmendment(char *buf, uint16_t offset, uint16_t len);
void CommodityDefinition(char *buf, uint16_t offset, uint16_t len);
void SeriesDefinitionExtended(char *buf, uint16_t offset, uint16_t len);
void SeriesDefinitionBase(char *buf, uint16_t offset, uint16_t len);
void ClassDefinition(char *buf, uint16_t offset, uint16_t len);
std::string trim(std::string s);

int main(int argc, char *argv[])
{
    int sockfd;
    int n;
    char buf[BUFF_SIZE];
    char read_buf[BUFF_SIZE];
    struct sockaddr_ll sll;
    struct ifreq ifr;
    struct ethhdr *eth;
    struct iphdr *iph;
    struct udphdr *udph;
    sockfd = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_IP));
    if (sockfd == -1)
    {
        printf("sockt error!\n");
        return -1;
    }
    strncpy(ifr.ifr_name, g_szIfName, IFNAMSIZ);
    if (ioctl(sockfd, SIOCGIFINDEX, &ifr) == -1) // 获取网卡信息
    {
        perror("ioctl()");
        close(sockfd);
        return -1;
    }
    memset(&sll, 0, sizeof(sll));
    sll.sll_family = PF_PACKET;
    sll.sll_ifindex = ifr.ifr_ifindex;
    sll.sll_protocol = htons(ETH_P_IP);
    if (bind(sockfd, (struct sockaddr *)&sll, sizeof(sll)) == -1)
    {
        perror("bind()");
        close(sockfd);
        return -1;
    }

    int i = 0;
    while (1)
    {
        // if (i++ == 10)
        //     break;
        n = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL);
        if (n == -1)
        {
            perror("recvfrom()");
            break;
        }
        eth = (struct ethhdr *)buf;
        iph = (struct iphdr *)(buf + sizeof(struct ethhdr));
        if (iph->protocol == IPPROTO_UDP)
        {
            udph = (struct udphdr *)(buf + sizeof(struct ethhdr) + sizeof(struct iphdr));
            char *packetPtr = buf + sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr);
            PacketHeader *hdr = (PacketHeader *)(packetPtr);
            char *msgPtr = packetPtr + 16; // packet header len = 16bytes
            char *ip = inet_ntoa(*(struct in_addr *)&iph->daddr);
            int port = ntohs(udph->dest);
            if (hdr->mMsgCount > 0)
            {
                if (strcmp(ip, "239.1.127.153") == 0 && port == 51002)
                {
                    // printf("PktSize:%hu,", hdr->mPktSize);
                    // printf("MsgCount:%hu,", hdr->mMsgCount);
                    // printf("SeqNum:%lu\n", hdr->mSeqNum);
                    ProcessMessageHeader(msgPtr, hdr->mMsgCount);
                }
            }
        }
    }
    close(sockfd);
    return 0;
}

std::string trim(std::string s)
{

    if (!s.empty())
    {
        s.erase(0, s.find_first_not_of(" "));
        s.erase(s.find_last_not_of(" ") + 1);
    }

    return s;
}

void ProcessMessageHeader(char *buf, int msgCount)
{
    int n = 0;
    int size = 0;
    while (n < msgCount)
    {
        MessageHeader *msghdr = (MessageHeader *)buf;
        // printf("msg %d: msgsize=%d, msgtype=%d\n", n, msghdr->mMsgSize, msghdr->mMsgType);
        if (msghdr->mMsgType == SERIESDEFINITIONBASE)
        {
            SeriesDefinitionBase(buf, 4, msghdr->mMsgSize);
        }
        // if (msghdr->mMsgType == ADDORDER)
        // {
        //     AddOrder(buf, 4, msghdr->mMsgSize);
        // }
        n++;
        buf = buf + msghdr->mMsgSize;
        size += msghdr->mMsgSize;
    }
    // printf("total size=%d\n", size + 16);
}

void AddOrder(char *buf, uint16_t offset, uint16_t len)
{
    XdpAddOrder addOrder(buf, len, offset);

    if (addOrder.orderbookId() == 21368738)
    {
        struct OrderData order;
        std::cout << "add," << addOrder.orderbookPosition() << ", " /*<< ret*/ << "," << order.price << "," << order.quantity << std::endl;

        // std::ofstream out("add.txt", std::ios::app);
        std::cout << "orderbookID:"
                  << addOrder.orderbookId() << std::endl;
        std::cout << "orderId:"
                  << addOrder.orderId() << std::endl;
        std::cout << "price:"
                  << addOrder.price() << std::endl;
        std::cout << "quantity:"
                  << addOrder.quantity() << std::endl;
        std::cout << "side:"
                  << +addOrder.side() << std::endl;
        std::cout << "lotType:"
                  << +addOrder.lotType() << std::endl;
        std::cout << "orderType:"
                  << addOrder.orderType() << std::endl;
        std::cout << "orderbookPosition:"
                  << addOrder.orderbookPosition() << std::endl;
        std::cout << "=====================================\n";
        // out << "=====================================\n";
        // out.close();
    }
}

void ModifyOrder(char *buf, uint16_t offset, uint16_t len)
{
    XdpModifyOrder modifyOrder(buf, len, offset);
    if (modifyOrder.orderbookId() == 56102818)
    {
        std::ofstream out("modify.txt", std::ios::app);
        out << "orderbookID:"
            << modifyOrder.orderbookId() << std::endl;
        out << "orderId:"
            << modifyOrder.orderId() << std::endl;
        out << "price:"
            << modifyOrder.price() << std::endl;
        out << "quantity:"
            << modifyOrder.quantity() << std::endl;
        out << "side:"
            << +modifyOrder.side() << std::endl;
        out << "orderType:"
            << modifyOrder.orderType() << std::endl;
        out << "orderbookPosition:"
            << modifyOrder.orderbookPosition() << std::endl;
        out << "=====================================\n";

        out.close();
    }
}

void DeleteOrder(char *buf, uint16_t offset, uint16_t len)
{
    XdpDeleteOrder deleteOrder(buf, len, offset);
    if (deleteOrder.orderbookId() == 56102818)
    {
        std::ofstream out("delete.txt", std::ios::app);
        out << "orderbookID:"
            << deleteOrder.orderbookId() << std::endl;
        out << "orderId:"
            << deleteOrder.orderId() << std::endl;
        out << "side:"
            << +deleteOrder.side() << std::endl;
        out << "=====================================\n";

        out.close();
    }
}

void ClearOrder(char *buf, uint16_t offset, uint16_t len)
{
    XdpClearOrder clearOrder(buf, len, offset);
    printf("OrderbookID:%u\n", clearOrder.orderbookId());
}

void Trade(char *buf, uint16_t offset, uint16_t len)
{
    XdpTrade trade(buf, len, offset);
    if (trade.orderbookId() == 56102818)
    {
        printf("tradeTime:%u\n", trade.tradeTime());
        printf("orderbookId:%u\n", trade.orderbookId());
        printf("orderId:%llu\n", trade.orderId());
        printf("tradeId:%llu\n", trade.tradeId());
        printf("comboGroupId:%u\n", trade.comboGroupId());
        printf("price:%d\n", trade.price());
        printf("quantity:%u\n", trade.quantity());
        printf("side:%hhu\n", trade.side());
        printf("dealType:%hhu\n", trade.dealType());
        printf("tradeCondition:%hu\n", trade.tradeCondition());
        printf("dealInfo:%hu\n\n", trade.dealInfo());
    }
}
void TradeAmendment(char *buf, uint16_t offset, uint16_t len)
{
    XdpTradeAmendment traAmend(buf, len, offset);
}

void CommodityDefinition(char *buf, uint16_t offset, uint16_t len)
{
    XdpCommodityDefinition commodity(buf, len, offset);
    // std::ofstream outfile("commodity.txt", std::ios::app);
    // outfile << "commodityCode:" << commodity.commodityCode() << std::endl;
    // outfile << "commodityId:" << commodity.commodityId() << std::endl;
    // outfile << "underlyingCode:" << commodity.underlyingCode() << std::endl;
    // outfile << "commodityName:" << commodity.commodityName() << std::endl;
    // outfile << "isinCode:" << commodity.isinCode() << std::endl;
    // outfile << "baseCurrency:" << commodity.baseCurrency() << std::endl;
    // outfile << "decimalUnderlyingPrice:" << commodity.decimalUnderlyingPrice() << std::endl;
    // outfile << "underlyingPriceUnit:" << +commodity.underlyingPriceUnit() << std::endl;
    // outfile << "nominalValue:" << commodity.nominalValue() << std::endl;
    // outfile << "underlyingType:" << +commodity.underlyingType() << std::endl;
    // outfile << "effectiveTomorrow:" << +commodity.effectiveTomorrow() << std::endl;
    // outfile << "===============================================================" << std::endl;
    // outfile.close();
    if (trim(commodity.commodityName()) == "HANG SENG INDEX")
    {
        std::cout << "commodityCode:" << commodity.commodityCode() << std::endl;
        std::cout << "commodityId:" << commodity.commodityId() << std::endl;
        std::cout << "underlyingCode:" << commodity.underlyingCode() << std::endl;
        std::cout << "commodityName:" << commodity.commodityName() << std::endl;
        std::cout << "isinCode:" << commodity.isinCode() << std::endl;
        std::cout << "baseCurrency:" << commodity.baseCurrency() << std::endl;
        std::cout << "decimalUnderlyingPrice:" << commodity.decimalUnderlyingPrice() << std::endl;
        std::cout << "underlyingPriceUnit:" << +commodity.underlyingPriceUnit() << std::endl;
        std::cout << "nominalValue:" << commodity.nominalValue() << std::endl;
        std::cout << "underlyingType:" << +commodity.underlyingType() << std::endl;
        std::cout << "effectiveTomorrow:" << +commodity.effectiveTomorrow() << std::endl;
        std::cout << "===============================================================" << std::endl;
    }
}

void SeriesDefinitionBase(char *buf, uint16_t offset, uint16_t len)
{
    XdpSeriesDefinitionBase sdb(buf, len, offset);
    // if (trim(sdb.symbol()) == "HSIZ8" || trim(sdb.symbol()) == "HSIG9" || trim(sdb.symbol()) == "HSIF9")

    std::string symbol = trim(sdb.symbol());
    if (symbol.substr(0, 3) == "HSI" && symbol.size() == 5 || (symbol.substr(0, 3) == "MHI" && symbol.size() == 5))
    {
        std::ofstream out("hkex.cfg", std::ios::app);
        out << trim(sdb.symbol()) << ","
            << sdb.orderbookId() << ","
            << sdb.numberOfDecimalsPrice() << ","
            << sdb.expirationDate() << ","
            << +sdb.numberOfLegs() << ","
            << +sdb.putOrCall() << std::endl;
        out.close();
        std::cout << "orderbookId:" << sdb.orderbookId() << std::endl;
        std::cout << "symbol:" << sdb.symbol() << std::endl;
        std::cout << "financialProduct:" << +sdb.financialProduct() << std::endl;
        std::cout << "numberOfDecimalsPrice:" << sdb.numberOfDecimalsPrice() << std::endl;
        std::cout << "numberOfLegs:" << +sdb.numberOfLegs() << std::endl;
        std::cout << "expirationDate:" << sdb.expirationDate() << std::endl;
        std::cout << "putOrCall:" << +sdb.putOrCall() << std::endl;
        std::cout << "===============================================================" << std::endl;
    }
}

void SeriesDefinitionExtended(char *buf, uint16_t offset, uint16_t len)
{
    XdpSeriesDefinitionExtented sde(buf, len, offset);
    if (trim(sde.symbol()) == "HSIV8")
    {
        std::cout << "orderbookId" << sde.orderbookId() << std::endl;
        std::cout << "symbol:" << sde.symbol() << std::endl;
        std::cout << "country" << +sde.country() << std::endl;
        std::cout << "market:" << +sde.market() << std::endl;
        std::cout << "instrumentGroup:" << +sde.instrumentGroup() << std::endl;
        std::cout << "modifier:" << +sde.modifier() << std::endl;
        std::cout << "commodityCode:" << sde.commodityCode() << std::endl;
        std::cout << "expirationDate:" << sde.expirationDate() << std::endl;
        std::cout << "strikePrice:" << sde.strikePrice() << std::endl;
        std::cout << "contractSize:" << +sde.contractSize() << std::endl;
        std::cout << "isinCode:" << sde.isinCode() << std::endl;
        std::cout << "seriesStatus:" << +sde.seriesStatus() << std::endl;
        std::cout << "effectiveTomorrow:" << +sde.effectiveTomorrow() << std::endl;
        std::cout << "priceQuotationFactor:" << sde.priceQuotationFactor() << std::endl;
        std::cout << "effectiveExpDate:" << sde.effectiveExpDate() << std::endl;
        std::cout << "dateTimeLastTrading:" << sde.dateTimeLastTrading() << std::endl;
        std::cout << "===============================================================" << std::endl;
    }
}

void ClassDefinition(char *buf, uint16_t offset, uint16_t len)
{
    XdpClassDefinition cd(buf, len, offset);

    if (cd.commodityCode() == 5306)
    {
        printf("country:%hhu\n", cd.country());
        printf("market:%hhu\n", cd.market());
        printf("instrumentGroup:%hhu\n", cd.instrumentGroup());
        printf("modifier:%hhu\n", cd.modifier());
        printf("commodityCode:%hu\n", cd.commodityCode());
        printf("expirationDate:%d\n", cd.priceQuotationFactor());
        printf("strikePrice:%u\n", cd.contractSize());
        printf("contractSize:%hu\n", cd.decimalInStrikePrice());
        printf("decimalInContractSize:%hu\n", cd.decimalInContractSize());
        printf("decimalInPremium:%hu\n", cd.decimalInPremium());
        printf("rnkingType:%hu\n", cd.rnkingType());
        printf("tradable:%hhu\n", cd.tradable());
        printf("premiumUnit4Price:%hhu\n", cd.premiumUnit4Price());
        std::cout << "baseCurrency:" << cd.baseCurrency() << std::endl;
        std::cout << "instrumentClassID:" << cd.instrumentClassID() << std::endl;
        std::cout << "instrumentClassName:" << cd.instrumentClassName() << std::endl;
        std::cout << "isFractions:" << cd.isFractions() << std::endl;
        std::cout << "settlementCurrencyID:" << cd.settlementCurrencyID() << std::endl;
        printf("effectiveTomorrow:%hhu\n", cd.effectiveTomorrow());
        printf("tickStepSize:%d\n\n", cd.tickStepSize());
        std::cout << "===============================================================" << std::endl;
    }
}