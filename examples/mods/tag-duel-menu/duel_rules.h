#ifndef TAG_DUEL_RULES_H
#define TAG_DUEL_RULES_H
/* Count completed individual turns from the actual opening seat, rather
 * than testing a named duelist. Tag: 4,9,12,17,20... Solo: 2,5,6,9,10... */
static int TagRules_FieldChangeDue(int completed, int seats)
{
    return completed >= seats &&
        (completed % (seats * 2) == seats || completed % (seats * 2) == 1);
}
/* The first individual turn, regardless of team or selected teammate. */
static int TagRules_OpeningAttacksBlocked(int active, int completed)
{
    return active && completed == 0;
}
/* Ring: player -> rival 1 -> partner -> rival 2. The next team's
 * first member is derived, never independently shuffled. */
static int TagRules_WaitingStarter(int first_side, int first_member)
{
    return first_side ? (first_member ^ 1) : first_member;
}
/* Map one fair roll to the five remaining field-card terrains. */
static int TagRules_NextTerrain(int current, unsigned roll)
{
    if (current < 1 || current > 6) return 1 + roll % 6;
    int next = 1 + roll % 5;
    return next >= current ? next + 1 : next;
}
#endif
