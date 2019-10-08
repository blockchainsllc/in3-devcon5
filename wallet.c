#include <in3/client.h>
#include <in3/eth_api.h>
#include <in3/eth_full.h>
#include <in3/in3_curl.h>
#include <in3/signer.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// helper functions

// reads the file content
static bytes_t read_file(char *path) {
  bytes_t res = bytes(NULL, 0);
  FILE *  f   = fopen(path, "r");
  if (f) {
    fseek(f, 0, SEEK_END);
    res.len = ftell(f);
    fseek(f, 0, SEEK_SET);
    res.data = malloc(res.len + 1);
    fread(res.data, 1, res.len, f);
    fclose(f);
    res.data[res.len] = 0;
  }
  return res;
}

// writes the content into a file.
static void write_file(char *path, bytes_t data) {
  FILE *file = fopen(path, "w");
  if (file) {
    fwrite(data.data, data.len, 1, file);
    fclose(file);
  } else
    printf("Error writing the file %s\n", path);
}

// cache-impl
static bytes_t *storage_get_item(void *cptr, char *key) {
  bytes_t data = read_file(key);
  return data.len ? b_dup(&data) : NULL;
}

void storage_set_item(void *cptr, char *key, bytes_t *content) {
  write_file(key, *content);
}


// creating the incubed-client
static in3_t *create_client() {

  // this needs to be done only once in order to register the verifier and transport.
  in3_register_curl();
  in3_register_eth_full();

  // create the client
  in3_t *c     = in3_new();

  // here we set the cache impl.
  in3_storage_handler_t *storage_handler =
      malloc(sizeof(in3_storage_handler_t));
  storage_handler->get_item = storage_get_item;
  storage_handler->set_item = storage_set_item;
  c->cacheStorage = storage_handler;

  return c;
}

static void show_help() { printf("wallet ...\n"); }
static void add_account(char *hex_address) {
  bytes_t old_account_data = read_file("accounts");
  uint8_t new_account_data[old_account_data.len + 20];
  if (old_account_data.len)
    memcpy(new_account_data, old_account_data.data, old_account_data.len);
  hex2byte_arr(hex_address, -1, new_account_data + old_account_data.len, 20);
  write_file("accounts", bytes(new_account_data, old_account_data.len + 20));
}
static void add_key(char *path) {
  bytes_t     keyfile = read_file(path);
  json_ctx_t *keyjson = parse_json(keyfile.data);
  char *      adr     = d_get_string(keyjson->result, "address");
  add_account(adr);
  write_file(adr, keyfile);
}

static void show_balances(char *token) {
  printf("balances:\n");

  in3_t *   c = create_client();
  address_t token_address;
  if (token) hex2byte_arr(token, -1, token_address, 20);

  bytes_t accounts = read_file("accounts");
  for (int i = 0; i < accounts.len; i += 20) {
    char hex_adr[43];
    bytes_to_hex(accounts.data + i, 20, hex_adr);
    printf(" Account 0x%s : ", hex_adr);
    if (token) {

      json_ctx_t *res =
          eth_call_fn(c, token_address, BLKNUM_LATEST(),
                      "balanceOf(address):(uint256)", accounts.data + i);
      if (!res)
        printf("Error getting balance : %s", eth_last_error());
      else
        printf("%lu token\n", d_long(res->result));
      free_json(res);
    } else {
      uint256_t balance = eth_getBalance(c, accounts.data + i, BLKNUM_LATEST());
      printf("%lu wei\n", as_long(balance));
    }
  }
}

static void send_transaction(char *keyfile, char *to_hex, uint64_t value) {

  address_t   to;
  address_t   from;
  bytes32_t   raw_key;
  in3_t *     c        = create_client();
  bytes_t     key_data = read_file(keyfile);
  json_ctx_t *key_json = parse_json(key_data.data);

  hex2byte_arr(to_hex, -1, to, 20);
  hex2byte_arr(keyfile, -1, from, 20);

  if (decrypt_key(key_json->result, getpass("Passphrase to unlock the key\n"),
                  raw_key) != IN3_OK) {
    printf("invalid key !\n");
    return;
  } else
    free_json(key_json);

  eth_set_pk_signer(c, raw_key);

  // sending a rpc-transaction
  char params[1000], *result = NULL, *error = NULL;
  sprintf(params, "[{\"from\":\"0x%s\", \"to\" : \"%s\", \"value\" : %lu}]",
          keyfile, to_hex, value);

  if (in3_client_rpc(c, "eth_sendTransaction", params, &result, &error) !=
          IN3_OK ||
      error) {
    printf("Error: %s", error);
    return;
  } else
    printf("succesasfully sent tx_hash=%s\n", result);

  bytes32_t tx_hash;
  hex2byte_arr(result, -1, tx_hash, 32);
  memset(raw_key, 0, 32);

  char *receipt = eth_wait_for_receipt(c, tx_hash);
  printf("confirmed with one block \n");

  free(receipt);
}

int main(int argc, char *argv[]) {
  in3_log_set_level(LOG_TRACE);
  char *cmd = argc > 1 ? argv[1] : "help";
  if (strcmp(cmd, "balance") == 0) show_balances(argc > 2 ? argv[2] : NULL);
  if (strcmp(cmd, "add_account") == 0) add_account(argv[2]);
  if (strcmp(cmd, "add_key") == 0) add_key(argv[2]);
  if (strcmp(cmd, "send") == 0)
    send_transaction(argv[2] + 2, argv[3], (uint64_t)atol(argv[4]));
  if (strcmp(cmd, "help") == 0) show_help();
}
