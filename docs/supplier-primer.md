# Working with a board supplier: a short primer

Written 2026-10-20 (PIZERO-243) for ordering more RP2350-PiZero boards from
Waveshare, possibly with the PSRAM chip fitted, and possibly under a supply
agreement. It is a primer, not legal or tax advice: for a contract with real
money in it, have a lawyer read it before you sign.

## 1. Know what you are asking for

Before the first email, write down four things. Suppliers answer clear
questions quickly and vague ones slowly.

- **The part:** the RP2350-PiZero, unchanged except for the PSRAM chip fitted
  at U1 (section 6), pre-flashed with our firmware if they offer that.
- **How many:** this order, and a realistic guess for the next year. Be honest:
  a supplier prices on the volume you will really buy.
- **When:** the date you need them in hand, counting shipping and customs.
- **What "good" means:** every board boots our firmware and shows a picture;
  with PSRAM, the chip is detected at its full size.

## 2. The vocabulary

| Term | Meaning |
|---|---|
| **RFQ** | Request for quotation: your written description of what you want, sent to get a price. |
| **Quote** | Their written offer: price per unit at each quantity, lead time, validity date. Keep it; it is the basis of any order. |
| **MOQ** | Minimum order quantity. A change to a stock board (fitting a part) usually has one, because someone sets up the line for it. |
| **NRE** | Non-recurring engineering: a one-off charge for setting up a custom variant. Fitting a part to an existing footprint should mean little or none; changing the board itself means more. |
| **BOM** | Bill of materials: the parts list. Adding the PSRAM chip changes the BOM. |
| **Lead time** | Weeks from paid order to shipped goods. Ask for it in writing, and ask what it depends on (usually a part's availability). |
| **Samples** | A few boards made the custom way, before a full order. Usually cost more each, sometimes free against a later order. |
| **Golden sample** | The sample you approve. Production must match it; it settles arguments later. |
| **PO** | Purchase order: your written order referring to the quote. Their acceptance makes it a contract. |
| **Payment terms** | When you pay. New small customers usually pay in full up front (often by bank transfer or PayPal); "30% deposit, 70% before shipping" is common for larger orders. "Net 30" (pay 30 days after delivery) is earned over time. |
| **Incoterms** | Who pays for and carries the risk of shipping. **EXW** (ex works): you collect. **FOB**: they deliver to the port. **DDP** (delivered duty paid): they deliver to your door with duties paid. For small orders, a courier quote "to your door" is simplest; ask whether duties are included. |
| **Duties and tariffs** | Import taxes. Boards made in China have been subject to changing US tariffs; check the current rate for the product's HS code before you price your kits. |
| **HS code** | The customs classification of the goods. The supplier puts it on the invoice; it sets the duty. |
| **PCN / EOL** | Product change notice / end of life notice: their promise to tell you before they change the board or stop making it. |
| **AQL** | Acceptable quality level: the share of defects allowed in a batch, and how many boards you may test to decide. |
| **RMA** | Return merchandise authorisation: how faulty boards are returned or credited. |

## 3. How a custom variant usually goes

1. **RFQ.** Email sales with the part, the change, the quantities (for example
   20, 100 and 500 to see the price steps) and your date.
2. **Quote.** Check the price at each quantity, the MOQ, any NRE, the lead
   time, how long the price holds, and the shipping terms.
3. **Samples.** Order a handful (5 to 10) built exactly as production will be.
   Pay for them if needed; it is the cheapest insurance in the process.
4. **Test and approve.** Run them on our firmware (section 6 has the tests).
   Approve one in writing as the golden sample, or send back what is wrong.
5. **Production order.** A PO referring to the quote and the golden sample.
6. **Inspect on arrival.** Test every board, or at least a fixed share, and
   report problems in writing within the agreed time.

## 4. Do you need a supply agreement?

At tens to a few hundred boards a year, usually not yet. A written quote, a
PO and clear emails cover most of what matters, and a supplier may not want
to negotiate a contract for a small account. Write things down anyway: an
email that says "confirmed: 100 boards with PSRAM fitted, $X each, ships by
date Y" is a record.

A **supply agreement** (also called a framework or master agreement) becomes
worth it when you order regularly, or when you depend on the custom variant
and could not quickly buy the same thing elsewhere. It usually covers:

- **Price:** per unit at each quantity, how long it holds, and how increases
  are announced (for example 60 days' notice).
- **Forecasts versus commitments:** a forecast is a guess you share so they
  can plan; a commitment is a promise to buy. Do not let a forecast quietly
  become a commitment.
- **Lead time and allocation:** how fast they ship, and who gets parts first
  when a chip is scarce.
- **Quality:** the golden sample, the tests, the AQL, and what happens to a
  bad batch (replace, refund, rework).
- **Changes and end of life:** a PCN before any change to the board or its
  parts, and an EOL notice with a last-time buy, so you are never surprised
  by a board that no longer runs the firmware.
- **Warranty and returns:** how long, and the RMA process.
- **Confidentiality:** an NDA if you share anything not already public. Our
  firmware is public, so there is little to protect here.
- **Exclusivity:** they will rarely give it to a small customer, and you
  should not promise it either.

## 5. Risks to keep in mind

- **One supplier.** The kit depends on one board from one maker. Keep a few
  spare boards, and know the stock board (no PSRAM) still runs everything we
  ship today.
- **The extra part.** PSRAM chips have had shortages before. Ask what they
  fit, and whether a substitute would be the same size and speed.
- **Silent changes.** A board revision can change a pin or a part. The PCN
  clause, and testing every batch, catch it.
- **The total landed cost:** unit price, plus shipping, plus duties, plus
  payment fees, plus the boards that fail. Price kits on that, not on the quote.

## 6. The PSRAM request in particular

**What the board already has.** The schematic shows an empty 8-pin footprint,
**U1, "PSRAM"**, on the same QSPI bus as the flash chip, with its chip select
on **GPIO 47** and a 10K pull-up (R39) already fitted. So this is a part
fitted to an existing footprint, not a new board, which should keep any NRE
small.

**What to ask for.** The usual chip for this footprint on RP2350 boards is an
**8 MB (64 Mbit) QSPI PSRAM in an 8-pin package**, such as AP Memory's
APS6404L-3SQR. The RP2350 can address up to 16 MB on that chip select, but
16 MB QSPI PSRAM in this small package is much less common. Ask Waveshare
which parts they can fit in this footprint and source reliably, rather than
asking for 16 MB and getting a substitution. 8 MB is far more than a 512 KB
CoCo 3 needs.

**What it would unlock.** The 512 KB CoCo 3, which this board cannot have now
(`coco3-feasibility.md`), and room to stop squeezing the SRAM.

**What is not known yet.** Whether the emulator stays at full speed with the
CoCo's RAM in PSRAM. PSRAM is much slower than the chip's own SRAM: it is read
through the same small cache as the flash, and every miss is a slow
transaction on the bus. A 1.79 MHz CoCo 3 touching 512 KB at random could miss
that cache often. It has to be measured on a real board before you order many.
That is the strongest reason to order samples first.

**What to test on the samples.** That the firmware detects the PSRAM at its
full size; a memory test across all of it; the stock firmware still running
unchanged; and then the speed measurement above, before committing to a
production quantity.

## 7. A first email

> Hello, I build a retro-computer kit around your RP2350-PiZero and would
> like to order more boards. Could you quote for the RP2350-PiZero with the
> PSRAM footprint (U1, chip select on GPIO 47) populated with an 8 MB QSPI
> PSRAM such as the APS6404L-3SQR, or the equivalent part you normally fit?
>
> Quantities: 10 samples first, then about [20 / 100 / 500] boards.
> Please include the unit price at each quantity, any minimum order or setup
> charge, lead time, shipping to [country] with duties if possible, and how
> long the price is valid. Could you also confirm which PSRAM part you would
> fit, and whether you can pre-flash our firmware (a UF2 file)?
>
> Thank you.

Keep everything in writing, and keep the quote, the golden sample and the
emails together. They are your agreement until you have a formal one.
