
/*
 * Copyright (C) 1997 Massachusetts Institute of Technology 
 *
 * This software is being provided by the copyright holders under the
 * following license. By obtaining, using and/or copying this software,
 * you agree that you have read, understood, and will comply with the
 * following terms and conditions:
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose and without fee or royalty is
 * hereby granted, provided that the full text of this NOTICE appears on
 * ALL copies of the software and documentation or portions thereof,
 * including modifications, that you make.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS," AND COPYRIGHT HOLDERS MAKE NO
 * REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED. BY WAY OF EXAMPLE,
 * BUT NOT LIMITATION, COPYRIGHT HOLDERS MAKE NO REPRESENTATIONS OR
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY PARTICULAR PURPOSE OR
 * THAT THE USE OF THE SOFTWARE OR DOCUMENTATION WILL NOT INFRINGE ANY
 * THIRD PARTY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. COPYRIGHT
 * HOLDERS WILL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE OR
 * DOCUMENTATION.
 *
 * The name and trademarks of copyright holders may NOT be used in
 * advertising or publicity pertaining to the software without specific,
 * written prior permission. Title to copyright in this software and any
 * associated documentation will at all times remain with copyright
 * holders. See the file AUTHORS which should have accompanied this software
 * for a list of all copyright holders.
 *
 * This file may be derived from previously copyrighted software. This
 * copyright applies only to those changes made by the copyright
 * holders listed in the AUTHORS file. The rest of this file is covered by
 * the copyright notices, if any, listed below.
 */

/* 
 + Look at partitioned udfs as multi-block datums. 
 + Solve the problem where a single byte contains a weird set of
   blocks (e.g., every other disk block in an extent)

 * Make udf's describe their own data structures.

	we have two names spaces that we control:
		0..n-1 the number of udfs (partitioned perhaps)
		0..sizeof (struct) ; word/byte/bit number in structure.
	should be able to exploit both (or just 2 and eliminate 1).

  Need: for a given update, what functions we have to check.
	functions can be bottom functs (give data) or meta functs
	(which say what funcs have to run).

  translated needs:
    For a given function, what it uses. (why?)
    For a given data, what functions it is used with.

  what about simple partitioning?  trouble is it has to look at data.
   solve butler's jumbled mess problem.

	problem : can't just run the entire update through.  have
	to be sure that it is an input we have had before.  so:
	give them a byte offset?  is this sufficient or do they need
	more?  e.g., if the update is to many things, they may need
	to see it to see what can be updated.

	process of udf application - paper shows naive implementation
	and follows each modification through, trying to demonastrate
	intuitions by example.

	ideally give in the update and they say what it modifies.  however,
	can't because it can change.  can they tell us? 
		assume we can put in update: they look at it and enumerate
		all functions that will be modified.  how to ensure that
		this is true?  simple: if the function we run looks at
		other stuff, then 

		cannot look at the data. can only look at the 
			(offset, len) <--- how do we know that this was
					an input?  track it I supose.

			can ask: offset: what is ok lengths?

	easy: for each byte, ask who depends on it.  you can't read
	unless you are in the depend set.  how to select which function
	to run?  clearly this is sufficitn, the question is is it efficient.

	when run, must run all that come up in turn to get enumerable
	set.  then after modification rerun all.

		adding a byte depedencency must be explicit since we have 
		to run the function before hand to see what it produces.

		deletion is ok; they are only hurting themselves.

		udf_offset: -> function used to run to see the natural
		byte size of this guy.

		we ask: offset give us bytes for this.
			it does.

		for each entry in the vector: 
			run function that takes offset:
				returns the number of expected bytes and 
					indices.

			if not sufficient, iterate through offset+nbytes.
			if too much, doesn't matter

			

	function that indicates guards: run before and after.
		those ops added to it must be re-run, 

 there is a real market for a portable, extensible linker; e.g., language
 design around the lack of support (c++), real lack of useful abilities.
 extensible = eat infrastructure once rather than each modification.
 can carry infrastructure with you.

  magik paper: discuss global linkage w/ seperate ocmpilation and why 
  ir instead of language.
 
  problem with overlap: 
	adding it requires checking.
	
 * Marketing problem: udf describes mechanism, good name probably
 * describes effect.
 */
