# INSERT INTO
Adds rows to the table. If the target table already exists and is not sorted, the operation `INSERT INTO` adds rows at the end of the table. For a sorted table, YQL tries to preserve sorting by starting a sorted merge.

Search for the table by name in the database specified by the [USE](use.md) operator.

`INSERT INTO` lets you perform the following operations:

* Adding constant values using [`VALUES`](values.md).

  ```yql
  INSERT INTO my_table (Column1, Column2, Column3, Column4)
  VALUES (345987,'ydb', 'Pied piper', 1414);
  COMMIT;
  ```

  ```yql
  INSERT INTO my_table (Column1, Column2)
  VALUES ("foo", 1), ("bar", 2);
  ```

* Saving the `SELECT` result.

  ```yql
  INSERT INTO my_table
  SELECT SourceTableColumn1 AS MyTableColumn1, "Empty" AS MyTableColumn2, SourceTableColumn2 AS MyTableColumn3
  FROM source_table;
  ```

## Using modifiers

Write operation can be performed with one or more modifiers. The modifier is specified after the `WITH` keyword after the table name: `INSERT INTO ... WITH SOME_HINT`. For example, to clear existing data from the table before the write operation, just add a modifier `WITH TRUNCATE`:

```yql
INSERT INTO my_table WITH TRUNCATE
SELECT key FROM my_table_source;
```

The following rules apply when working with modifiers:

- If the modifier has a value, it's specified after the `=` sign: `INSERT INTO ... WITH SOME_HINT=value`.
- If you need to specify multiple modifiers, they must be enclosed in parentheses: `INSERT INTO ... WITH (SOME_HINT1=value, SOME_HINT2, SOME_HINT3=value)`.

The full list of supported write modifiers:

* `TRUNCATE` to clear existing data from the table.
* `COMPRESSION_CODEC=codec_name` to write data with the specified compression codec. Takes priority over the `yt.PublishedCompressionCodec` pragma. For permitted values, see the [Compression]({{yt-docs-root}}/user-guide/storage/compression) section.
* `ERASURE_CODEC=erasure_name` to write data with the specified erasure codec. Takes priority over the `yt.PublishedErasureCodec` pragma. For permitted values, see the [Erasure coding]({{yt-docs-root}}/user-guide/storage/replication#erasure) section.
* `EXPIRATION=value` to the set expiration period for the table being created. The value can be set as `duration` or as `timestamp` in [ISO 8601](https://en.wikipedia.org/wiki/ISO_8601) format. Takes priority over the `yt.ExpirationDeadline`/`yt.ExpirationInterval` pragmas. This modifier can only be set together with `TRUNCATE`.
* `REPLICATION_FACTOR=factor` to set the replication factor for the table being created. The value must be in the [1, 10] range.
* `USER_ATTRS=yson` to set the user attribute that will be added to the table. Attributes must be set as a yson dictionary.
* `MEDIA=yson` to set the media. Takes priority over the `yt.PublishedMedia` pragma. For permitted values, see the [Media]({{yt-docs-root}}/user-guide/storage/media) section.
* `PRIMARY_MEDIUM=medium_name` to set the primary medium. Takes priority over the `yt.PublishedPrimaryMedium` pragma.
* `KEEP_META` to save the current schema, user attributes, and retention attributes (codecs, replications, media) of the target table. This modifier can only be set together with `TRUNCATE`.

{% note info %}

We do not recommend using system attributes in `USER_ATTRS` because the result of their application to the written data is not defined.

{% endnote %}


#### Examples

```yql
INSERT INTO my_table WITH (TRUNCATE, EXPIRATION="15m")
SELECT key FROM my_table_source;

INSERT INTO my_table WITH USER_ATTRS="{attr1=value1; attr2=value2;}"
SELECT key FROM my_table_source;
```


