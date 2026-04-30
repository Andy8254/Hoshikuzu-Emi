#include "core/CommandRegistry.hpp"

/*
 This section will be for commands related to brackets AS WELL AS tournament-related commands in general,
such as tournament registraion, match reportion etc. Due to the prohibiting price of Challonge's API for
grass-root tournaments, this programmer has decided to implemetn a custom tournament management system
for the bot with a focus on TETR.IO tournaments (later applicable to PPT2, TE:C, NES TETRIS, Tetra eSports
etc.).
 The tournament management system will include features such as:
 Format Creation and Management
 - Single Elimination, Double Elimination, Round Robin, Swiss etc.
 - Custom Seeding and Randomisation Options (This recycles muse918's Google Colab code)
 - Match Reporting and Result Management
 - Current Tournament Status of Players (Round Number, opponent information, etc.)
 - Tournament Registration and Management (This may require a CSV database or a Google Sheet integration
   for ease of use - without the need for SQL or any other complex database management system)
 - Bracket Visualisation (This may be implemented using a third-party library or a custom solution, depending
 on the complexity and requirements of the visualisation.)
 - Exporting Player Seed Info into a CSV file, therefore making back-ups w/ Challonge, Battle.fy etc. available
*/

void register_bracket_commands(dpp::cluster& bot) {

}