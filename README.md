# Ring

Fork of the repository for the source code of the engine presented at the paper Worst-case Optimal Graph Joins in Almost No Space.

Contains the dynamic implementation of the Ring with a compact dictionary for the graph and query translation.

## Instructions

To run our code, **we have to install an extended version of the library SDSL**. Go to [this repository](https://github.com/darroyue/sdsl-lite) and follow the instructions.

We also need to install the extended version of the **DYNAMIC library**. Go to [this repository](https://github.com/yuval-linker/DYNAMIC) and follow the instructions.
This repository is included as a github submodule in the `external` folder. After downloading the submodule

```Bash
cd external/DYNAMIC
mkdir build
cd build
cmake ..
make hopscotch_map
```

After the extended version of SDSL and DYNAMIC are installed, we have to clone this repository and follow these steps:

1. Create our `build` folder and compile the code:
```Bash
mkdir build
cd build
cmake ..
make
```

Check that we do not have any errors.

2. Download the version of the translated Wikidata that we want to use:

- [Translated Wikidata Filtered (about 80M triples)](http://compact-leapfrog.tk/files/wikidata-filtered-enumerated.dat).
- [Translated Wikidata (about 1000M triples)](http://compact-leapfrog.tk/files/wikidata-enumerated.dat.gz). Note that we have to decompress this file.

Now put the .dat file inside a folder.

Or get the original filtered Wikidata file:

- [Original Wikidata Filtered (about 80M triples)](https://zenodo.org/record/4035223/files/wikidata-wcg-filtered.nt.bz2?download=1)

3. Now we have to create the index. After compiling the code we should have an executable called `build-index`. Now run:

```Bash
./build-index <absolute-path-to-the-.dat-file> <type-of-ring>
```

This will generate some files in the folder where the `.dat` file is located. **Please keep all the files in the same folder**.

4. We are ready to run the code! We should have another executable file called `query-index`, then we should run:

```Bash
./query-index <absoulute-path-to-the-.dat-file> <absolute-path-to-the-query-file>
```

Here we need to give the path of the the file that contains all the queries. Besides the `Ring` folder we should find another folder called `Queries`. We have to give the path of one of the files within it:

- If we selected the file `wikidata-filtered-enumerated.dat` we have to give the absolute path of the file called `Queries-wikidata-benchmark.txt`.
- If we selected the file `wikidata-enumerated.dat` we have to select the absolute path of the file called `Queries-wikidata-benchmark.txt`.

There are other files for modification of the index. Obviously this actions only work on the dynamic version of the Ring:

- `insert-edge.cpp`: Inserts all the triples in a file to the index (It doesn't save it).

- `delete-edge.cpp`: Deletes all the triples in a file from the index.

- `delete-node.cpp`: Deletes all the triples with a value $s$ or $o$ equal to the ones in the file (It doesn't save it).

Now we are finished! After running this step we will execute the queries. In console we should see the number of the query, the number of results and the time taken by each one of the queries.

5. **[OPTIONAL]** If we would want to run the `CRing` code instead, you should [download this version of our source code](http://compact-leapfrog.tk/files/CRing.zip). All the steps are equivalent.

---

At the moment, we can find the rest of the complementary material at [this webpage](http://compact-leapfrog.tk/). Note that we will find instructions to run the code there, and although the instructions are different from the ones in this repository, they should work too.
