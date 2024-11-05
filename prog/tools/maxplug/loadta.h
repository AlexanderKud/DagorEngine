// Copyright (C) Gaijin Games KFT.  All rights reserved.
#pragma once

struct TexAnimFile
{
  Tab<char *> tex;
  Tab<int> frm;

  ~TexAnimFile();
  void clear();
  void add_frame(char *);
  bool parse(char *);
  bool load(const char *fn);
  static char *getlasterr();
};
