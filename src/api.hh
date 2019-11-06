/*
 * liquidsfz - sfz sampler
 *
 * Copyright (C) 2019  Stefan Westerfeld
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef LIQUIDSFZ_LIQUIDSFZ_HH
#define LIQUIDSFZ_LIQUIDSFZ_HH

#include <memory>

namespace LiquidSFZ
{

class Synth
{
  struct Impl;
  std::unique_ptr<Impl> impl;

public:
  Synth();
  ~Synth();

  void set_sample_rate (uint sample_rate);
  bool load (const std::string& filename);
  void add_midi_event (uint offset, unsigned char *midi_data);
  void process (float **outputs, uint nframes);
};

}

#endif
