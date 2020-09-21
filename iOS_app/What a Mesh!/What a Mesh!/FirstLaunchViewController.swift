//
//  ViewController.swift
//  What a Mesh!
//
//  Created by Gabriele Giuli on 2020-02-08.
//  Copyright © 2020 GabrieleGiuli. All rights reserved.
//

import UIKit

class FirstLaunchViewController: UIViewController {
    
    @IBOutlet weak var firstName: LoginTextField!
    @IBOutlet weak var lastName: LoginTextField!

    override func viewDidLoad() {
        super.viewDidLoad()
        // Do any additional setup after loading the view.
    }
    
    override var prefersStatusBarHidden: Bool {
        return true
    }

    @IBAction func UserTapped(_ sender: Any) {
        if !isValidName(firstName.text!) || !isValidName(lastName.text!) {
            let alert = UIAlertController(title: "Check your Name", message: "The names you have just entered are not valid, re-enter them and retry", preferredStyle: UIAlertController.Style.alert)
            alert.addAction(UIAlertAction(title: "Mhh... Sure!", style: UIAlertAction.Style.default, handler: nil))
            self.present(alert, animated: true, completion: nil)
        } else {
            let newId = getNewID();
            UserDefaults.standard.set(newId, forKey: "USER_ID")
            UserDefaults.standard.set(firstName.text!, forKey: "USER_FIRSTNAME")
            UserDefaults.standard.set(lastName.text!, forKey: "USER_LASTNAME")
            
            let alert = UIAlertController(title: "Success!", message: "You are all set. Connect to the WAM network!", preferredStyle: UIAlertController.Style.alert)
            alert.addAction(UIAlertAction(title: "Ok", style: UIAlertAction.Style.default, handler: { UIAlertAction in
                
                let storyboard = UIStoryboard(name: "Main", bundle: nil)
                let messagesViewController = storyboard.instantiateViewController(withIdentifier: "NavView") as! UINavigationController
                self.present(messagesViewController, animated: true, completion: nil)
        
            }))
            self.present(alert, animated: true, completion: nil)
        }
        
    }
    

    func isValidName(_ name: String) -> Bool {
        let nameRegEx = "(?<! )[-a-zA-Z' ]{2,26}"
        
        let namePred = NSPredicate(format:"SELF MATCHES %@", nameRegEx)
        return namePred.evaluate(with: name)
    }
    
    func getNewID() -> String {
        let date = Date()
        let calender = Calendar.current
        let components = calender.dateComponents([.year,.month,.day,.hour,.minute,.second], from: date)
        
        let year = components.year
        let month = components.month
        let day = components.day
        let hour = components.hour
        let minute = components.minute
        let second = components.second
        
        let randomNumber = Int.random(in: 0 ..< 10000)
        
        let today_string = String(year!) + String(month!) + String(day!) + String(hour!) + String(minute!) + String(second!) + String(randomNumber)
        
        return today_string
    }
    
}

