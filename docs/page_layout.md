# 🗄️ Database Page Layout Specification

## 📋 Overview
- **PAGE_SIZE:** 4096 bytes
- **Byte Order:** Little-endian

---

## 1. 💾 Page 0: Superblock (Meta-page)
| Offset | Size | C Type     | Description |
| :---   | :--- | :---       | :--- |
| 0      | 10   | `char[10]` | Magic Number ("rdbms-c-v\0"). |
| 10     | 4    | `uint32_t` | Format Version. |
| 14     | 2    | `uint16_t` | Page Size (4096 bytes). |
| 16     | 4    | `uint32_t` | System Catalog Root Page. |
| 20     | 4    | `uint32_t` | Free List Head.  (List of Free Pages) |
| 24     | 4072 | -          | Reserved. |

**Total Size:** 24 bytes

---

## 2. 🗂️ System Catalog Payload (Table/Index Metadata)
**System Catalog Leaf Node Cell Specific Data**

| Relative Offset | Size | C Type | Description
| :--- | :--- | :--- | :--- |
| 0 | 1 | `uint8_t` | Type (`0` for Table, `1` for Index) |
| 1 | 64 | `char[64]` | Table/Index Name |
| 65 | 4 | `uint32_t` | Root Table/Index Page Num
| 69 | Dynamic | `char[]` | SQL Query for Schema

**Total Size:** (69 + length of Schema) bytes

---

## 3. 🏷️ B+Tree Common Node Header

| Offset | Size | C Type     | Description |
| :---   | :--- | :---       | :--- |
| 0      | 1    | `uint8_t`  | Node Type: `0` (Leaf), `1` (Internal). |
| 1      | 1    | `uint8_t`  | Is Root: `1` (Yes), `0` (No). |
| 2      | 4    | `uint32_t` | Parent Pointer (Parent's `page_num`) |
| 6      | 2    | `uint16_t` | Cell Count<br><small>(Internal node's IDs or Page Data)</small> |
| 8      | 2    | `uint16_t` | Free Space Offset<br><small>(Last write position)</small>

**Common Header Size:** 10 bytes

---

## 4. 🍃 B+Tree Leaf Node Layout
| Offset | Size | C Type     | Description |
| :---   | :--- | :---       | :--- |
| 0      | 10   | -          | **Common Header** |
| 10     | 4    | `uint32_t` | Previous Leaf Pointer. |
| 14     | 4    | `uint32_t` | Next Leaf Pointer. |
| 18 | `Cell Count * 2` | `uint16_t[]` |  Cell Pointer Array<br><small>(Find specific cell content immediately despite having dynamic size using offset pointers)</small>
| `Free Space Offset` | Dynamic | Dynamic | Cell Content<br><small>**[Page Data]**<br>(Written bottom-up allowing dynamic sized data without conflicts with amount of cell pointers)</small>

**Total Leaf Header Size:** 18 bytes

---

## 5. 🌿 B+Tree Internal Node Layout
| Offset | Size | C Type     | Description |
| :---   | :--- | :---       | :--- |
| 0      | 10   | -          | **Common Header** |
| 10     | 4    | `uint32_t` | Rightmost Child Pointer. |
| 14 | `Cell Count * 2` | `uint16_t[]` |  Cell Pointer Array<br><small>(Find specific cell <i>IDs</i> immediately despite having dynamic size using offset pointers)</small>
| `Free Space Offset` | Dynamic | Dynamic | Cell Content<br><small>**[Child Pointer + Key]**<br>(Written bottom-up allowing dynamic sized data without conflicts with amount of cell pointers)</small>

**Total Internal Header Size:** 14 bytes

---

## 6. ♻️ Free Page Layout
When Page gets truncated, its getting put on the Free Page List for quick acccess and usage.
| Offset | Size | C Type     | Description |
| :---   | :--- | :---       | :--- |
| 0      | 4    | `uint32_t` | Next Free Page Pointer (`0` if it is the last). |
| 4      | 4092 | -          | Garbage/Old Leftover Data |