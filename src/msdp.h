#define MSDP_VAR			1
#define MSDP_VAL			2
#define MSDP_TABLE_OPEN		3
#define MSDP_TABLE_CLOSE	4
#define MSDP_ARRAY_OPEN		5
#define MSDP_ARRAY_CLOSE	6

#define MSDP_TABLE	3
#define MSDP_ARRAY  4

struct msdp_node {
	int isa;
	int maxsize;
	char ** key;
	struct msdp_node *value;
};
