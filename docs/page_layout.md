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

## 2. 🗂️ System Catalog Payload
**System Catalog Leaf Node Cell Specific Data**

The object identity is stored in the composite Catalog B+Tree key and
is therefore not duplicated inside the Catalog payload.

| Relative Offset | Size | C Type | Description
| :--- | :--- | :--- | :--- |
| 0 | 1 | `uint8_t` | Type (`0` for Table, `1` for Index) |
| 1 | 4 | `uint32_t` | Root Table/Index Page Num
| 5 | 4 | `uint32_t` | Metadata Page Num<br><small>(Where Table/Schema/Column/Constraint/Index metadata is stored)</small>

**Total Size:** 9 bytes

<details>
  <summary><h3>🗝️ Unique System Catalog Key Format <small>(Click to expand)</small></h3></summary>

System Catalog B+Tree is going to be traversed using a **composite 3-value key**<br>
using the format below:

```
(table_name, object_type, object_name)

---

("users", TABLE, "")
("users", INDEX, "pk_users")
("users", INDEX, "idx_email")

("orders", TABLE, "")
("orders", INDEX, "idx_email")
```

</details>

<details>
  <summary><h3>📃 Metadata Page Header <small>(Click to expand)</small></h3></summary>

Metadata Pages are used to store all serialized Table/Index Metadata separately from System Catalog Leaf Cells, preventing System Catalog Leaf Node overflow. 

If a single Metadata Page cannot store all serialized Table/Index Metadata, additional Metadata Pages are allocated in memory through the Pager, linked together as a linked list and later stored in disk.


| Offset | Size | C Type     | Description |
| :---   | :--- | :---       | :--- |
| 0      | 4    | `uint32_t` | Next Metadata Page Pointer (`UINT32_MAX` if it is the last). |
| 4      | 4092 | -          | Table/Index Metadata |

</details>

---

## 3. 🏷️ B+Tree Common Node Header

| Offset | Size | C Type     | Description |
| :---   | :--- | :---       | :--- |
| 0      | 1    | `uint8_t`  | Node Type: `0` (Leaf), `1` (Internal). |
| 1      | 1    | `uint8_t`  | Is Root: `1` (Yes), `0` (No). |
| 2      | 4    | `uint32_t` | Parent Pointer (Parent's `page_num`) |
| 6      | 2    | `uint16_t` | Cell Count<br><small>(Number of cells stored in the node)</small> |
| 8      | 2    | `uint16_t` | Free Space Offset<br><small>(Last write position)</small>

**Common Header Size:** 10 bytes

---

## 4. 🍃 B+Tree Leaf Node Layout
| Offset | Size | C Type     | Description |
| :---   | :--- | :---       | :--- |
| 0      | 10   | -          | **Common Header** |
| 10     | 4    | `uint32_t` | Previous Leaf Pointer. |
| 14     | 4    | `uint32_t` | Next Leaf Pointer. |
| 18 | `Cell Count * 4` | `uint32_t[]` |  Cell Pointer Array<br><small>(Contains `offset` and `length` of cells)</small>
| `Free Space Offset` | Dynamic | Dynamic | Cell Content<br><small>**[Keys + Row] / [Keys + Catalog Payload]**<br>(Written bottom-up allowing dynamic sized data without conflicts with amount of cell pointers)</small>

**Total Leaf Header Size:** 18 bytes

---

## 5. 🌿 B+Tree Internal Node Layout
| Offset | Size | C Type     | Description |
| :---   | :--- | :---       | :--- |
| 0      | 10   | -          | **Common Header** |
| 10     | 4    | `uint32_t` | Rightmost Child Pointer. |
| 14 | `Cell Count * 4` | `uint32_t[]` |  Cell Pointer Array<br><small>(Contains `offset` and `length` of cells)</small>
| `Free Space Offset` | Dynamic | Dynamic | Cell Content<br><small>**[Child Pointer + Keys]**<br>(Written bottom-up allowing dynamic sized data without conflicts with amount of cell pointers)</small>

**Total Internal Header Size:** 14 bytes

---

## 6. ♻️ Free Page Layout
When a page is released, it is added to the Free Page List for quick reuse.
| Offset | Size | C Type     | Description |
| :---   | :--- | :---       | :--- |
| 0      | 4    | `uint32_t` | Next Free Page Pointer (`0` if it is the last). |
| 4      | 4092 | -          | Garbage/Old Leftover Data |