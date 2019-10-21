/*
 * liquidsfz - sfz support using fluidsynth
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

#include "loader.hh"

#include <algorithm>
#include <regex>

#include <assert.h>

using std::string;
using std::vector;
using std::regex;
using std::regex_replace;

static string
strip_spaces (const string& s)
{
  static const regex lws_re ("^\\s*");
  static const regex tws_re ("\\s*$");

  string r = regex_replace (s, tws_re, "");
  return regex_replace (r, lws_re, "");
}

void
Loader::set_key_value (const string& key, const string& value)
{
  if (region_type == RegionType::NONE)
    return;

  Region& region = (region_type == RegionType::REGION) ? active_region : active_group;
  printf ("+++ '%s' = '%s'\n", key.c_str(), value.c_str());
  if (key == "sample")
    {
      // on unix, convert \-seperated filename to /-separated filename
      string native_filename = value;
      std::replace (native_filename.begin(), native_filename.end(), '\\', fs::path::preferred_separator);

      region.sample = fs::absolute (sample_path / native_filename);
    }
  else if (key == "lokey")
    region.lokey = convert_key (value);
  else if (key == "hikey")
    region.hikey = convert_key (value);
  else if (key == "key")
    {
      const int k = convert_key (value);
      region.lokey = k;
      region.hikey = k;
      region.pitch_keycenter = k;
    }
  else if (key == "lovel")
    region.lovel = convert_key (value);
  else if (key == "hivel")
    region.hivel = convert_key (value);
  else if (key == "pitch_keycenter")
    region.pitch_keycenter = convert_key (value);
  else if (key == "lorand")
    region.lorand = convert_float (value);
  else if (key == "hirand")
    region.hirand = convert_float (value);
  else if (key == "loop_mode")
    region.loop_mode = convert_loop_mode (value);
  else if (key == "loop_start")
    region.loop_start = convert_int (value);
  else if (key == "loop_end")
    region.loop_end = convert_int (value);
  else if (starts_with (key, "locc"))
    {
      int cc = convert_int (key.substr (4));
      if (cc >= 0 && cc <= 127)
        region.locc[cc] = convert_int (value);
    }
  else if (starts_with (key, "hicc"))
    {
      int cc = convert_int (key.substr (4));
      if (cc >= 0 && cc <= 127)
        region.hicc[cc] = convert_int (value);
    }
  else if (starts_with (key, "on_locc") || starts_with (key, "on_hicc"))
    region.trigger = Trigger::CC;
  else if (key == "trigger")
    region.trigger = convert_trigger (value);
  else if (key == "seq_length")
    region.seq_length = convert_int (value);
  else if (key == "seq_position")
    region.seq_position = convert_int (value);
  else if (key == "ampeg_delay")
    region.ampeg_delay = convert_float (value);
  else if (key == "ampeg_attack")
    region.ampeg_attack = convert_float (value);
  else if (key == "ampeg_decay")
    region.ampeg_decay = convert_float (value);
  else if (key == "ampeg_sustain")
    region.ampeg_sustain = convert_float (value);
  else if (key == "ampeg_release")
    region.ampeg_release = convert_float (value);
  else
    printf ("unsupported opcode '%s'\n", key.c_str());
}

bool
Loader::parse (const string& filename)
{
  sample_path = fs::path (filename).remove_filename();

  FILE *file = fopen (filename.c_str(), "r");
  assert (file);

  // read file
  vector<string> lines;

  string input;
  int ch = 0;
  while ((ch = fgetc (file)) >= 0)
    {
      if (ch != '\r' && ch != '\n')
        {
          input += ch;
        }
      else
        {
          if (ch == '\n')
            {
              lines.push_back (input);
              input = "";
            }
        }
    }
  if (!input.empty())
    lines.push_back (input);
  fclose (file);

  this->filename = filename;

  // strip comments
  static const regex comment_re ("//.*$");
  for (auto& l : lines)
    l = regex_replace (l, comment_re, "");

  static const regex space_re ("\\s+(.*)");
  static const regex tag_re ("<([^>]*)>(.*)");
  static const regex key_val_re ("([a-z0-9_]+)=(\\S+)(.*)");
  for (auto l : lines)
    {
      line_count++;
      while (l.size())
        {
          std::smatch sm;
          //printf ("@%s@\n", l.c_str());
          if (regex_match (l, sm, space_re))
            {
              l = sm[1];
            }
          else if (regex_match (l, sm, tag_re))
            {
              handle_tag (sm[1].str());
              l = sm[2];
            }
          else if (regex_match (l, sm, key_val_re))
            {
              string key = sm[1];
              string value = sm[2];
              if (key == "sample" || key == "sw_label")
                {
                  /* parsing sample=filename is problematic because filename can contain spaces
                   * we need to handle three cases:
                   *
                   * - filename extends until end of line
                   * - filename is followed by <tag>
                   * - filename is followed by key=value
                   */
                  static const regex key_val_space_re_eol ("[a-z0-9_]+=([^=<]+)");
                  static const regex key_val_space_re_tag ("[a-z0-9_]+=([^=<]+)(<.*)");
                  static const regex key_val_space_re_eq ("[a-z0-9_]+=([^=<]+)(\\s[a-z0-9_]+=.*)");
                  if (regex_match (l, sm, key_val_space_re_eol))
                    {
                      set_key_value (key, strip_spaces (sm[1].str()));
                      l = "";
                    }
                  else if (regex_match (l, sm, key_val_space_re_tag))
                    {
                      set_key_value (key, strip_spaces (sm[1].str()));
                      l = sm[2]; // parse rest
                    }
                  else if (regex_match (l, sm, key_val_space_re_eq))
                    {
                      set_key_value (key, strip_spaces (sm[1].str()));
                      l = sm[2]; // parse rest
                    }
                  else
                    {
                      fail ("sample/sw_label opcode");
                      return false;
                    }
                }
              else
                {
                  set_key_value (key, value);
                  l = sm[3];
                }
            }
          else
            {
              fail ("toplevel parsing");
              return false;
            }
        }
    }
  if (!active_region.empty())
    regions.push_back (active_region);

  for (size_t i = 0; i < regions.size(); i++)
    {
      const auto cached_sample = sample_cache.load (regions[i].sample);
      regions[i].cached_sample = cached_sample;

      if (regions[i].loop_mode == LoopMode::DEFAULT)
        {
          if (cached_sample->loop)
            {
              regions[i].loop_mode = LoopMode::CONTINUOUS;
              regions[i].loop_start = cached_sample->loop_start;
              regions[i].loop_end = cached_sample->loop_end;
            }
          else
            {
              regions[i].loop_mode = LoopMode::NONE;
            }
        }
      printf ("loading %.1f %%\r", (i + 1) * 100.0 / regions.size());
      fflush (stdout);
    }
  printf ("\n");
  printf ("*** regions: %zd\n", regions.size());
  return true;
}