#include <stdio.h>
#include <stdlib.h>

#include "board_tree.h"
#include "constants.h"
#include "buckets.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "hand_value_tree.h"
#include "io.h"
#include "params.h"
#include "rollout.h"

using namespace std;

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <game params> <card params> <street> <features name> "
	  "<squashing> [wins|wmls] <num threads> \n", prog_name);
  fprintf(stderr, "\nSquashing of 1.0 means no squashing\n");
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc < 8) Usage(argv[0]);
  Files::Init();
  unique_ptr<Params> game_params = CreateGameParams();
  game_params->ReadFromFile(argv[1]);
  Game::Initialize(*game_params);
  unique_ptr<Params> card_params = CreateCardAbstractionParams();
  card_params->ReadFromFile(argv[2]);
  unique_ptr<CardAbstraction>
    card_abstraction(new CardAbstraction(*card_params));
  int street;
  if (sscanf(argv[3], "%i", &street) != 1) Usage(argv[0]);
  string features_name = argv[4];
  double squashing;
  if (sscanf(argv[5], "%lf", &squashing) != 1) Usage(argv[0]);
  bool wins;
  string warg = argv[6];
  if (warg == "wins")      wins = true;
  else if (warg == "wmls") wins = false;
  else                     Usage(argv[0]);
  int num_threads;
  if (sscanf(argv[7], "%i", &num_threads) != 1) Usage(argv[0]);
  
  HandValueTree::Create();
  // Need this for ComputeRollout()
  BoardTree::Create();
  Buckets buckets(*card_abstraction, false);
  int num_features = buckets.NumBuckets(0);
  float *pct_vals = OppoClusterComputeRollout(street, wins, &buckets, num_threads);

  unsigned int num_boards = BoardTree::NumBoards(street);
  unsigned int num_hole_card_pairs = Game::NumHoleCardPairs(street);
  unsigned int num_hands = num_boards * num_hole_card_pairs;
  fprintf(stderr, "%u hands\n", num_hands);

  char buf[500];
  sprintf(buf, "%s/features.%s.%u.%s.%u", Files::StaticBase(), Game::GameName().c_str(),
	  Game::NumRanks(), features_name.c_str(), street);
  Writer writer(buf);
  writer.WriteInt(num_features);

  for (unsigned int h = 0; h < num_hands; ++h) {
    for (int p = 0; p < num_features; ++p) {
      writer.WriteFloat(pct_vals[h * num_features + p]);
    }
  }
}
