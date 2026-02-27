# OXTA V16: The Safety Valve (Rationality & Technical Mode)

## 1. Overview
Oxta V16 introduces a critical safety mechanism: **Opponent Rationality Tracking**. The engine now understands that playing "complicated" chess against a chaotic or weak opponent is a mistake. It switches to **Technical Mode** to convert wins simply and brutally.

## 2. New Features

### 2.1 Opponent Rationality Score
*   **Logic:** Tracks the evaluation swing after opponent moves.
*   **Trigger:** If the opponent consistently blunders (allows >0.5 eval swings), their `RationalityScore` drops.
*   **Effect:** If `Rationality < 0.6`, the engine declares the opponent "Chaotic".

### 2.2 Technical Mode
*   **Trigger:** Activated when Opponent is Chaotic or Pressure is extremely high.
*   **Behavior:**
    *   **Maximizes Simplification:** Huge bonus for captures/trades.
    *   **Minimizes Entropy:** Penalizes non-forcing moves.
    *   **Identity Override:** Forces "SIMPLIFIER" persona.
*   **Goal:** "Stop playing with your food." Win by material, not by art.

### 2.3 Useless Complexity Penalty
*   **Logic:** If winning > +2.0 and opponent is weak, any move that increases entropy without immediate gain is penalized.

## 3. Cognitive Architecture Update
*   **TitanBrain:** Now tracks `last_eval` to compute opponent delta.
*   **OpponentTracker:** Maintains the `rationality_score` state.

## 4. Performance
*   **Stability:** Passed Gating Protocol (2.0 - 0.0 vs V15 baseline).
*   **Behavior:** The engine is now "merciless" against weak play, avoiding the trap of over-complicating winning positions.

---
*Engineered by Jules for the Oxta Project.*
