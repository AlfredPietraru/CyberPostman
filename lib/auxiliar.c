#include "auxiliar.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#define ETHER_SIZE 14
#define IPHDR_SIZE 20
#define ICMPHDR_SIZE 8

arp_table *init_arp_table(char *arp_pathname)
{
	arp_table *my_arp_table = malloc(sizeof(arp_table));
	my_arp_table->vector = malloc(MAX_ADDR_NR * sizeof(struct arp_entry));
	my_arp_table->size = parse_arp_table(arp_pathname, my_arp_table->vector);
	return my_arp_table;
}

arp_table *init_arp_table_without_file() {
	arp_table *my_arp_table = malloc(sizeof(arp_table));
	my_arp_table->vector = malloc(MAX_ADDR_NR * sizeof(struct arp_entry));
	my_arp_table->size  = 0;
	return my_arp_table;
}

void free_arp_table(arp_table *my_arp_table)
{
	free(my_arp_table->vector);
	free(my_arp_table);
}

router_table *init_router_table(const char *pathname)
{
	router_table *rt_tbl = malloc(sizeof(router_table));
	rt_tbl->vector = malloc(sizeof(struct route_table_entry) * MAX_ADDR_NR);
	rt_tbl->size = read_rtable(pathname, rt_tbl->vector);
	return rt_tbl;
}

void free_router_table(router_table *rt_tbl)
{
	free(rt_tbl->vector);
	free(rt_tbl);
}

void swap(struct route_table_entry *entry1, struct route_table_entry *entry2)
{
	struct route_table_entry aux_entry;
	aux_entry.interface = entry1->interface;
	aux_entry.mask = entry1->mask;
	aux_entry.next_hop = entry1->next_hop;
	aux_entry.prefix = entry1->prefix;

	entry1->interface = entry2->interface;
	entry1->mask = entry2->mask;
	entry1->next_hop = entry2->next_hop;
	entry1->prefix = entry2->prefix;

	entry2->interface = aux_entry.interface;
	entry2->mask = aux_entry.mask;
	entry2->next_hop = aux_entry.next_hop;
	entry2->prefix = aux_entry.prefix;
}

int compare(struct route_table_entry current, struct route_table_entry pivot)
{
	if (pivot.prefix > current.prefix)
	{
		return 1;
	}
	if (pivot.prefix < current.prefix)
	{
		return 0;
	}
	if (pivot.prefix == current.prefix)
	{
		if (pivot.mask > current.mask)
		{
			return 0;
		}
		if (pivot.mask < current.mask)
		{
			return 1;
		}
	}
	return 0;
}

int partition(router_table *rt_tbl, int low, int high)
{
	struct route_table_entry pivot = rt_tbl->vector[high];
	int i = (low - 1);

	for (int j = low; j <= high - 1; j++)
	{
		// rt_tbl->vector[j].prefix < pivot.prefix
		if (compare(rt_tbl->vector[j], pivot))
		{
			i++;
			swap(&rt_tbl->vector[i], &rt_tbl->vector[j]);
		}
	}
	swap(&rt_tbl->vector[i + 1], &rt_tbl->vector[high]);
	return (i + 1);
}

void quickSort(router_table *rt_tbl, int low, int high)
{
	if (low < high)
	{
		int pi = partition(rt_tbl, low, high);
		quickSort(rt_tbl, low, pi - 1);
		quickSort(rt_tbl, pi + 1, high);
	}
}

void print_ip_addr(uint32_t addr)
{
	unsigned char value[4];
	memcpy(value, &addr, sizeof(addr));
	for (int i = 0; i < 4; i++)
	{
		printf("%d ", value[i]);
	}
	printf("\n");
}

void print_mac_addr(uint8_t vector[6])
{
	for (int i = 0; i < 6; i++)
	{
		printf("%x ", vector[i]);
	}
	printf("\n");
}

void liniar_sorting(router_table *rt_tbl)
{
	for (int i = 0; i < rt_tbl->size; i++)
	{
		for (int j = 0; j < rt_tbl->size; j++)
		{
			if (compare(rt_tbl->vector[i], rt_tbl->vector[j]))
			{
				swap(&rt_tbl->vector[i], &rt_tbl->vector[j]);
			}
		}
	}
}

int liniar_search(router_table *rt_tbl, uint32_t ddar)
{
	int last_index = -1;
	uint32_t last_biggest_prefix = 0;
	for (int i = 0; i < rt_tbl->size; i++)
	{
		if ((rt_tbl->vector[i].mask & ddar) == rt_tbl->vector[i].prefix)
		{
			if (last_biggest_prefix < rt_tbl->vector[i].prefix) {
				last_index = i;
				last_biggest_prefix = rt_tbl->vector[i].prefix;
			}
		}
	}
	return last_index;
}

int verify_checksum(char *buf, struct iphdr *ip_hdr)
{
	uint16_t oldchecksum = ntohs(ip_hdr->check);
	ip_hdr->check = 0;
	uint16_t new_checksum = checksum((uint16_t *)ip_hdr, sizeof(struct iphdr));
	if (new_checksum == oldchecksum)
	{
		return 1;
	}
	return 0;
}

struct package* create_package(int interface, size_t len, char *buf) {
	struct package *pack = malloc(sizeof(struct package));
			pack->buf = malloc(len);
			memcpy(pack->buf, buf, len);
			pack->interface = interface;
			pack->len = len;
	return pack;
}

char *create_arp_request(struct route_table_entry *rt_tbl_entry, struct iphdr* ip_hdr) {
		char *buffer = malloc(sizeof(struct ether_header) + sizeof(struct arp_header));

			struct ether_header *my_eth = (struct ether_header*)(buffer);
			my_eth->ether_type = ntohs(0x0806);
			for(int i =0 ; i < 6; i++) {
				my_eth->ether_dhost[i] = 0xff;
			}
			uint8_t src_mac[6];
			uint8_t init_sender_mac[6];
			get_interface_mac(rt_tbl_entry->interface, src_mac);
			memcpy(init_sender_mac, my_eth->ether_shost, 6);
			memcpy(my_eth->ether_shost, src_mac, 6);
		    
			struct arp_header *my_arp = (struct arp_header*)(buffer + sizeof(struct ether_header));
			my_arp->op = htons(1);
			my_arp->hlen = 6;
			my_arp->htype = htons(1);
			my_arp->plen = 4;
			my_arp->ptype = htons(0x0800);
			memcpy(my_arp->sha, my_eth->ether_shost, 6);
			
			struct in_addr addr;
			inet_aton(get_interface_ip(rt_tbl_entry->interface), &addr);
			my_arp->spa = addr.s_addr;

			for(int i =0; i < 6; i++) {
				my_arp->tha[i] = 0x00;
			}  

			my_arp->tpa = ip_hdr->daddr;
	return buffer;
}

 char *create_icmp_header(char *buf, int interface, uint8_t type) 
 {
	char *buffer = malloc(ETHER_SIZE + 
				2 * IPHDR_SIZE + ICMPHDR_SIZE + 8);
	struct ether_header* my_ether = (struct ether_header*)buffer;
	get_interface_mac(interface, my_ether->ether_shost);
	struct ether_header *old_ether = (struct ether_header*)buf;
	memcpy(my_ether->ether_dhost, old_ether->ether_shost, 6);
	my_ether->ether_type = htons(0x0800);

	struct iphdr* my_iphdr = (struct iphdr*)(buffer + ETHER_SIZE);
	struct iphdr* old_iphdr = (struct iphdr*)(buf + ETHER_SIZE);

	struct in_addr addr_in;
	inet_aton(get_interface_ip(interface), &addr_in);
	memcpy(my_iphdr, old_iphdr, ICMPHDR_SIZE);

	my_iphdr->daddr = old_iphdr->saddr;
	my_iphdr->saddr = (uint32_t)addr_in.s_addr;

	my_iphdr->protocol = 1;
	my_iphdr->ttl = 64;
	my_iphdr->frag_off = htons(0x40);
	my_iphdr->tot_len = htons(ICMPHDR_SIZE + IPHDR_SIZE);
	my_iphdr->frag_off = htons(my_iphdr->frag_off);

	my_iphdr->check = 0;
	my_iphdr->check = htons(ntohs(checksum((uint16_t *)my_iphdr, IPHDR_SIZE)));
	
	struct icmphdr* icmp_hdr = (struct icmphdr*)(buffer + ETHER_SIZE + IPHDR_SIZE);
	icmp_hdr->code = 0;
	icmp_hdr->type = type;
	//ceva nou
	memcpy(buffer + ETHER_SIZE + IPHDR_SIZE + ICMPHDR_SIZE, old_iphdr, IPHDR_SIZE);

	icmp_hdr->checksum = 0;
	icmp_hdr->checksum = htons(checksum((uint16_t*)(buffer + ETHER_SIZE + IPHDR_SIZE), 
	ICMPHDR_SIZE + IPHDR_SIZE + 8));
	return buffer;
}

