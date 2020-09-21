//
//  ChatsViewController.swift
//  What a Mesh!
//
//  Created by Gabriele Giuli on 2020-02-08.
//  Copyright © 2020 GabrieleGiuli. All rights reserved.
//

import UIKit
import Alamofire
import SwiftyJSON
import MessengerKit

class ChatsViewController: UITableViewController {

    var connected: Bool = false;
    var address = "192.168.4.1"
    var available_users: [ParsedUser] = []
    var this_user: ParsedUser?
    var selected_user_id: String?
    
    var fwdVC: ConversationViewController?
    
    override func viewDidLoad() {
        super.viewDidLoad()
        
        print("VIEWDIDLOAD")
        
        let name = UserDefaults.standard.string(forKey: "USER_FIRSTNAME")!
        let surname = UserDefaults.standard.string(forKey: "USER_LASTNAME")!
        let id = UserDefaults.standard.string(forKey: "USER_ID")!
        this_user = ParsedUser(name: name, ID: id)
        title = name + " " + surname + "'s Conversations"
        
        Timer.scheduledTimer(timeInterval: 3, target: self, selector: #selector(refreshUsers), userInfo: nil, repeats: true)

    }

    // MARK: - Table view data source

    override func numberOfSections(in tableView: UITableView) -> Int {
        // #warning Incomplete implementation, return the number of sections
        return 1
    }

    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        // #warning Incomplete implementation, return the number of rows
        return self.available_users.count
    }

    
    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        let cell = tableView.dequeueReusableCell(withIdentifier: "StandardCell", for: indexPath)
        
        cell.textLabel?.text = available_users[indexPath.row].name
        cell.detailTextLabel?.text = available_users[indexPath.row].messages.last

        return cell
    }
    
    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        self.selected_user_id = self.available_users[indexPath.row].ID
        self.performSegue(withIdentifier: "SegueID", sender: self.available_users[indexPath.row])
    }
    
    @objc func refreshUsers() {
        print(self.available_users)
        
        if !connected {
            performHandshake()
        }
        
        let requestString = "http://\(self.address)/update_data"
        Alamofire.request(requestString).responseJSON(completionHandler: { response in
            if let json = try? JSON(data: response.data!) {
                print(json)
                self.parseUsers(json: json)
            } else {
                print("Error in JSON")
            }
            
        })
    }
    
    @objc func performHandshake() {
        
        let name = UserDefaults.standard.string(forKey: "USER_FIRSTNAME")!
        let surname = UserDefaults.standard.string(forKey: "USER_LASTNAME")!
        let id = UserDefaults.standard.string(forKey: "USER_ID")!
        
        let requestString = "http://\(self.address)/client_name:" + name + "/client_id:" + id;
        Alamofire.request(requestString)
    }
    
    func parseUsers(json: JSON) {
        for message in json["Messages"].arrayValue {
            let sender_id = message["Source ID"].stringValue
            let message_text = message["Message"].stringValue
            
            self.addMessage(message_text: message_text, user_id: sender_id)
        }
        
        for user in json["Data"].arrayValue {
            let user_id = user["ID"].stringValue
            let user_name = user["Name"].stringValue
            
            insertUser(user: ParsedUser(name: user_name, ID: user_id))
        }
    }
    
    func insertUser(user: ParsedUser) {
        if !isUserPresent(input_user: user) {
            if user.ID != self.this_user!.ID {
                self.available_users.append(user)
                self.tableView.reloadData()
                
            } else {
                self.connected = true
            }
        }
        
    }
    
    func isUserPresent(input_user: ParsedUser) -> Bool {
        for user in self.available_users {
            if user.ID == input_user.ID {
                return true;
            }
        }
        
        return false;
    }
    
    func addMessage(message_text: String, user_id: String) {
        print("adding " + message_text)
        for user in self.available_users {
            if user_id == user.ID {
                user.messages.append(message_text)
                self.tableView.reloadData()
                
                if let vc = self.fwdVC, let id = self.selected_user_id {
                    if user_id == id {
                    vc.id += 1
                    
                    let body: MSGMessageBody = (message_text.containsOnlyEmoji && message_text.count < 5) ? .emoji(message_text) : .text(message_text)
                    
                    let message = MSGMessage(id: vc.id, body: body, user: vc.tim, sentAt: Date())
                    vc.insert(message)
                }
                }
            }
        }
    }
    
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        let vc = segue.destination as! ConversationViewController
        vc.recipient = sender as? ParsedUser
        vc.addMessagesAtBeginning()
        self.fwdVC = vc
    }
    
    
    @IBAction func refresh(_ sender: Any) {
        self.available_users = []
        self.tableView.reloadData()
        connected = false;
    }
    

    /*
    // Override to support conditional editing of the table view.
    override func tableView(_ tableView: UITableView, canEditRowAt indexPath: IndexPath) -> Bool {
        // Return false if you do not want the specified item to be editable.
        return true
    }
    */

    /*
    // Override to support editing the table view.
    override func tableView(_ tableView: UITableView, commit editingStyle: UITableViewCell.EditingStyle, forRowAt indexPath: IndexPath) {
        if editingStyle == .delete {
            // Delete the row from the data source
            tableView.deleteRows(at: [indexPath], with: .fade)
        } else if editingStyle == .insert {
            // Create a new instance of the appropriate class, insert it into the array, and add a new row to the table view
        }    
    }
    */

    /*
    // Override to support rearranging the table view.
    override func tableView(_ tableView: UITableView, moveRowAt fromIndexPath: IndexPath, to: IndexPath) {

    }
    */

    /*
    // Override to support conditional rearranging of the table view.
    override func tableView(_ tableView: UITableView, canMoveRowAt indexPath: IndexPath) -> Bool {
        // Return false if you do not want the item to be re-orderable.
        return true
    }
    */

    /*
    // MARK: - Navigation

    // In a storyboard-based application, you will often want to do a little preparation before navigation
    override func prepare(for segue: UIStoryboardSegue, sender: Any?) {
        // Get the new view controller using segue.destination.
        // Pass the selected object to the new view controller.
    }
    */

}
