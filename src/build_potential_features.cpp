#include <stdio.h>
#include <stdlib.h>

#include "board_tree.h"
#include "buckets.h"
#include "card_abstraction.h"
#include "card_abstraction_params.h"
#include "constants.h"
#include "files.h"
#include "game.h"
#include "game_params.h"
#include "hand_value_tree.h"
#include "io.h"
#include "params.h"
#include "rollout.h"

using namespace std;

static void Usage(const char *prog_name) {
  fprintf(stderr,
          "USAGE: %s <game params> <card params> <street> <features name> "
          "<pct 0> <pct 1>... <pct n>\n",
          prog_name);
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
  unique_ptr<CardAbstraction> card_abstraction(
      new CardAbstraction(*card_params));
  int street;
  if (sscanf(argv[3], "%i", &street) != 1) Usage(argv[0]);
  string features_name = argv[4];

  int num_percentiles = argc - 5;
  double *percentiles = new double[num_percentiles];
  for (int i = 0; i < num_percentiles; ++i) {
    if (sscanf(argv[5 + i], "%lf", &percentiles[i]) != 1) Usage(argv[0]);
  }

  HandValueTree::Create();
  // Need this for ComputeRollout()
  BoardTree::Create();
  Buckets buckets(*card_abstraction, false);
  int *pct_vals =
      ComputePotentialRollout(street, percentiles, num_percentiles, &buckets);

  size_t num_boards = BoardTree::NumBoards(street);
  size_t num_hole_card_pairs = Game::NumHoleCardPairs(street);
  size_t num_hands = num_boards * num_hole_card_pairs;
  fprintf(stderr, "%u hands\n", num_hands);

  char buf[500];
  sprintf(buf, "%s/features.%s.%u.%s.%u", Files::StaticBase(),
          Game::GameName().c_str(), Game::NumRanks(), features_name.c_str(),
          street);
  Writer writer(buf);
  writer.WriteInt(num_percentiles);

  for (size_t h = 0; h < num_hands; ++h) {
    for (int p = 0; p < num_percentiles; ++p) {
      writer.WriteInt(pct_vals[h * num_percentiles + p]);
    }
  }
}
