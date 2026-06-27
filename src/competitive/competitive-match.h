#pragma once

class DuelManager;

bool isCompetitiveMatchActive();
void setCompetitiveMatchActive(bool active, int opponentMmr = 5000);
int getCompetitiveOpponentMmr();
void onCompetitiveMatchEnd(DuelManager& duel, bool playerWon);
