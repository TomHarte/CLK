//
//  NewDocumentViewController.swift
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa

private class Machine {
	init(name: String, imageName: String) {
		self.name = name
		self.imageName = imageName
	}
	var name: String = ""
	var imageName: String = ""
}

class NewDocumentCollectionViewItem: NSCollectionViewItem {
	override var representedObject: Any? {
		didSet {
			if let machine = representedObject as? Machine, let imageView = imageView, let textField = textField {
				imageView.image = NSImage(named: machine.imageName)
				textField.stringValue = machine.name
			}
		}
	}

/*	override var selected: Bool {
		didSet {
			print("\(selected)")
			var colour: NSColor!
			var lineColour: NSColor!
 
			if selected {
				colour		= NSColor.selectedControlColor()
				lineColour	= NSColor.blackColor()
			} else {
				colour		= NSColor.controlBackgroundColor()
				lineColour	= NSColor.controlBackgroundColor()
			}

			if let box = self.view as? NSBox {
				box.borderColor = lineColour
				box.fillColor = colour
			}
		}
	}*/
}

class NewDocumentViewController: NSViewController, NSCollectionViewDelegate {
	@IBOutlet var collectionView : NSCollectionView!
	fileprivate var collectionViewItems : [Machine]!

    override func viewDidLoad() {
        super.viewDidLoad()

		collectionView.minItemSize = NSSize(width: 200, height: 180)

		let itemCopy: NewDocumentCollectionViewItem! = NewDocumentCollectionViewItem().copy(with: nil) as! NewDocumentCollectionViewItem
		collectionView.itemPrototype = itemCopy
		collectionView.content = [
			Machine(name: "Acorn Electron", imageName: "ElectronBASIC"),
			Machine(name: "Commodore Vic-20", imageName: "Vic20BASIC")
		]
    }

	@IBAction func cancel(_ sender : AnyObject!) {
		self.view.window?.close()
	}

	@IBAction func choose(_ sender : AnyObject!) {
		print("\(collectionView.selectionIndexes)")
	}
}
