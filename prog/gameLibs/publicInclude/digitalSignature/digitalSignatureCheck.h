//
// Dagor Engine 6.5 - Game Libraries
// Copyright (C) Gaijin Games KFT.  All rights reserved.
//
#pragma once

bool verify_digital_signature_with_key(const void **buffers, const unsigned *buf_sizes, const unsigned char *signature,
  int signature_size, const unsigned char *public_key, int public_key_len);

bool verify_digital_signature_with_key_and_algo(const char *hash_algorithm, const void **buffers, const unsigned *buf_sizes,
  const unsigned char *signature, int signature_size, const unsigned char *public_key, int public_key_len);
