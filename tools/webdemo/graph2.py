#!python

from time import *

port = 49998

class Graph:
	def __init__ (self, x, y, width, color):
		self.size = 0
		self.xLoc = x
		self.yLoc = y
		self.width = width
		self.color = color
		self.done = 1

class Title:
	def __init__ (self, y, t):
		self.loc, self.text = y, t

class myScale:
	def __init__ (self, pos, width, max):
		self.pos = pos
		self.width = width
		self.maxRange = max

class GraphCollection:
	# private data members
	graphs = []
	interPairSpacing = 0
	intraPairSpacing = 0
	number = 0
	titles = []
	scales = []
	width = 0
	
	# public function members
	def __init__ (self, numPairs, titles, legends, maxRange, color1, color2, totalHeight,
		      totalWidth, spacing1, spacing2):

		self.number = numPairs
		self.interPairSpacing = spacing1
		self.intraPairSpacing = spacing2
		self.legend = legends

		scaleHeight = .25
		legendHeight = 1.5

		# compute width of each graph
		totalHeight = totalHeight - (scaleHeight + legendHeight + (self.interPairSpacing * (self.number - 1)))
		perPairWidth = totalHeight / self.number

		# create all graphs at proper locations
		yStart = .5
		for x in range(self.number):
			self.graphs.append (Graph (0, yStart, perPairWidth, color1))
			yStart = yStart + perPairWidth/2;
			self.titles.append (Title (yStart, titles[x]))
			yStart = yStart + perPairWidth/2;
			yStart = yStart + self.interPairSpacing
		self.scales.append (myScale (yStart-.1, totalWidth, maxRange))
	# get nth graph of a group
	def nth (self, group, which):
		return (self.graphs[which-1])

from Tkinter import *

class Display (Frame):
	def createGraphs (self, numPairs, titles, legends):
		self.graphs = GraphCollection (numPairs, titles, legends, self.maxRange,
					       'red', 'blue', 
					       self.height, self.width,
					       0.5, 0.25)

	def incrementGraph (self, group, which, increment):
		graph = self.graphs.nth (group, which)
		graph.size = graph.size + increment
		self.repaintSingle (group, which, increment)

	def enable (self, which):
		graph = self.graphs.nth (0, which)
		graph.done = 0

	def stopDrawing (self):
		self.prevTime = 0

	def startDrawing (self):
		self.prevTime = time ()

	def tick (self):
		if self.prevTime == 0:
			return

		t = time ()
		inc = t - self.prevTime
		self.prevTime = t
		for graph in self.graphs.graphs:
			if not graph.done:
				graph.size = graph.size + inc
				self.repaintSingle2 (graph, inc)

	def done (self, group, which):
		graph = self.graphs.nth (group, which)
		graph.done = 1

	def repaintSingle2 (self, graph, amount):
		x1 = str (1.25+self.scaleFactor*(graph.size-amount))+"i"
		y1 = str (graph.yLoc )+"i"
		x2 = str (1.25+self.scaleFactor * graph.size)+"i"
		y2 = str (graph.yLoc + graph.width)+"i"
		foo = self.draw.create_rectangle (x1, y1, x2, y2,
						  {"fill": graph.color,
						   "outline": ""})
		self.deleteList.append (foo)
		self.tk.update ()

	def repaintSingle (self, group, which, amount):
		graph = self.graphs.nth (group, which)
		self.repaintSingle2 (graph, amount)

	def repaintAll (self):
		for graph in self.graphs.graphs:
			x1 = "1.25i"
			y1 = str (graph.yLoc )+"i"
			x2 = str (1.25+self.scaleFactor*graph.size)+"i"
			y2 = str (graph.yLoc + graph.width)+"i"
			foo = self.draw.create_rectangle (x1, y1, x2, y2, 
							  {"fill": graph.color,
							   "outline": ""})
			self.deleteList.append (foo)

		for title in self.graphs.titles:
			self.draw.create_text (".1i", str (title.loc)+"i",
					       {'text': title.text,
						'font': '*-times-medium-r-normal--*-240-*-*-*-*-*-*',
						'anchor': 'w'})	

		for scale in self.graphs.scales:
			y = scale.pos
			x1 = 1.25
			x2 = 1.25+self.scaleFactor * scale.maxRange

			increment = scale.maxRange / scale.width
			percent = 0
			prevX = x1
			while x1 <= x2:
				self.draw.create_line (str (prevX)+"i", str (y)+"i", str (x1)+"i", str(y)+"i",
						       str (x1)+"i", str(y-.1)+"i")
				self.draw.create_text (str (x1)+"i", str (y)+"i",
						       {'text': '%d' % percent,
							'font': '*-times-medium-r-normal--*-180-*-*-*-*-*-*',
							'anchor': 'n'})
				prevX = x1
				percent = percent + increment
				x1 = x1 + self.scaleFactor * increment


		legendY = self.height-.5;
		self.draw.create_text (str ((self.width+1)/2)+"i", str (legendY)+"i",
				       {'text': 'Elapsed time (seconds)',
					'font': '*-times-medium-r-normal--*-180-*-*-*-*-*-*',
					'anchor': 'c'})
		self.draw.create_text (str ((self.width+1)/2)+"i", ".25i",
					{'text': self.graphs.legend[0],
					 'font': '*-times-medium-r-normal--*-240-*-*-*-*-*-*',
					 'anchor': 'c'})
			
		self.tk.update ()

	def createWidgets(self):
		self.QUIT = Button(self.tk, {'text': 'QUIT', 'fg': 'red', 'command': self.quit})
		self.QUIT.pack({'side': 'bottom', 'fill': 'both'})	

		self.draw = Canvas(self.tk, {"width" : str (self.width+1)+"i", 
					     "height" : str (self.height)+"i"})
		self.draw.pack({'side': 'left'})
		self.tk.title ("Exo Perf-O-Meter")

	def __init__ (self, maxRange, height, width, master=None):
		self.height = height
		self.width = width
		self.maxRange = maxRange
		self.scaleFactor = (self.width) / self.maxRange
		self.deleteList = []
		self.prevTime = 0
		self.tk = Tk ()
		self.createWidgets ()

from socket import *
from string import *

def getStartup ():
	global sock
	global barrierCount

	barrierCount = 0
	sock = socket (AF_INET, SOCK_DGRAM)
	sock.bind (("", port))
	initString = sock.recvfrom (512)
	words = splitfields (initString[0], "_")
	return ((words[0], words[1], splitfields (words[2], "."),
		 splitfields (words[3], "."), words[4], words[5], words[6]))

def doOp ():
	global sock
	global barrierCount
	global barrierMach1
	global barrierMach2
	global barrierMach3
	global numGraphsEnabled

	command = sock.recvfrom (512)
	addr = command[1]
	command = splitfields (command[0], " ")
	args = command[1:]
	command = command[0]

	if command == 'increment':
		display.incrementGraph (atoi (args[0]),
					atoi (args[1]),
					atoi (args[2]))
		return 0

	elif command == 'enable':
		numGraphsEnabled = numGraphsEnabled + 1
		display.enable (atoi (args[0]))
		return 0

	elif command == 'barrier1d':
		sock.sendto ("1", addr)
		display.startDrawing ()
		barrierCount = 0
		return 0

        elif command == 'barrier2':
                barrierCount = barrierCount + 1
                if barrierCount == 1:
                        barrierMach1 = addr
                elif barrierCount == 2:
                        sock.sendto ("1", barrierMach1)
                        sock.sendto ("1", addr)
                        display.startDrawing ()
                        barrierCount = 0
                return 0

        elif command == 'barrier2d':
                barrierCount = barrierCount + 1
                if barrierCount == 1:
                        barrierMach1 = addr
                elif barrierCount == 2:
                        sock.sendto ("1", barrierMach1)
                        sock.sendto ("1", addr)
                        display.startDrawing ()
                        barrierCount = 0
                return 0

        elif command == 'barrier3':
                barrierCount = barrierCount + 1
                if barrierCount == 1:
                        barrierMach1 = addr
                elif barrierCount == 2:
                        barrierMach2 = addr
                elif barrierCount == 3:
                        sock.sendto ("1", barrierMach2)
                        sock.sendto ("1", barrierMach1)
                        sock.sendto ("1", addr)
                        barrierCount = 0
                return 0

        elif command == 'barrier3d':
                barrierCount = barrierCount + 1
                if barrierCount == 1:
                        barrierMach1 = addr
                elif barrierCount == 2:
                        barrierMach2 = addr
                elif barrierCount == 3:
                        sock.sendto ("1", barrierMach2)
                        sock.sendto ("1", barrierMach1)
                        sock.sendto ("1", addr)
                        display.startDrawing ()
                        barrierCount = 0
                return 0

        elif command == 'barrier4':
                barrierCount = barrierCount + 1
                if barrierCount == 1:
                        barrierMach1 = addr
                elif barrierCount == 2:
                        barrierMach2 = addr
                elif barrierCount == 3:
                        barrierMach3 = addr
                elif barrierCount == 4:
                        sock.sendto ("1", barrierMach3)
                        sock.sendto ("1", barrierMach2)
                        sock.sendto ("1", barrierMach1)
                        sock.sendto ("1", addr)
                        barrierCount = 0
                return 0

        elif command == 'barrier4d':
                barrierCount = barrierCount + 1
                if barrierCount == 1:
                        barrierMach1 = addr
                elif barrierCount == 2:
                        barrierMach2 = addr
                elif barrierCount == 3:
                        barrierMach3 = addr
                elif barrierCount == 4:
                        sock.sendto ("1", barrierMach3)
                        sock.sendto ("1", barrierMach2)
                        sock.sendto ("1", barrierMach1)
                        sock.sendto ("1", addr)
                        display.startDrawing ()
                        barrierCount = 0
                return 0

	elif command == 'done':
		display.done (atoi (args[0]),
			      atoi (args[1]))
		numGraphsEnabled = numGraphsEnabled - 1
		if numGraphsEnabled == 0:
			display.stopDrawing ()
		return 1

	else:
		print "Unknown command: ", command
		return (0)

title, numberOfPairs, titles, legends, maxRange, width, height = getStartup ()
display = Display (atoi (maxRange), atof (width), atof (height)+0.25)
display.createGraphs (atoi (numberOfPairs), titles, legends)
display.repaintAll ()

from select import *

done = 0
ticks = 0
numGraphsEnabled = 0
totalGraphs = atoi (numberOfPairs) * 1
while done < totalGraphs:
	ready = select ([sock], [], [], 0);
	if ready[0] != []:
		done = done + doOp ()
	if time () > ticks + .1:
		display.tick ()
		ticks = time ()

display.mainloop ()
